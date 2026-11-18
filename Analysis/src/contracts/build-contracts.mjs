import fs from "node:fs";
import path from "node:path";
import {
  analysisDir,
  files,
  generatedDir,
} from "../shared/paths.mjs";
import { readJson, stableId, uniqueSorted, writeJson, writeText } from "../shared/io.mjs";
import {
  loadParsedIndex,
  objectLeafName,
  packagePathFromObjectPath,
  propertyRows,
  resolveDependencyGraph,
  softObjectPaths,
} from "./asset-model.mjs";

const FIELD_GROUPS = {
  projectile: new Set(["ProjectileClass", "Projectile", "ProjectileTemplate", "ProjectileComponent"]),
  component: new Set(["ComponentClass", "DamageComponent", "ProjectileComponent"]),
  status: new Set(["StatusEffect", "TriggeredStatusEffect", "OwnerEffect", "TargetStatusEffect"]),
  input: new Set(["InputAction", "TriggerClusterHoldDuration", "SpecialReload", "TriggerClusterActive"]),
  lifecycle: new Set(["LifeSpan", "Lifetime", "Duration", "MaxLifetime", "DestroyDelay"]),
  spawn: new Set(["GrenadeFragments", "FragmentCount", "NumProjectiles", "ProjectileCount", "PelletCount"]),
  tick: new Set(["TickInterval", "DamageInterval", "UpdateInterval", "PulseInterval"]),
  area: new Set(["AoERange", "Radius", "DamageRadius", "AreaRadius"]),
  replication: new Set(["ServerOnly", "Replicated", "ReplicateMovement"]),
};

const CLASS_TOKENS = {
  projectileReplacement: ["WeaponSwitchProjectileUpgrade", "CryoSprayProjectileUpgrade"],
  componentAddition: ["AddComponentUpgrade"],
  eventBinding: ["WeaponHitCounterUpgrade", "TriggeredStatusEffectUpgrade"],
};

function hasClass(rows, names) {
  return rows.some((row) => names.some((name) => JSON.stringify(row).includes(name)));
}

function valuesFor(rows, fields) {
  return rows.filter((row) => fields.has(row.name)).map((row) => ({
    field: row.name,
    value: row.value,
    evidenceRef: row.evidenceRef,
  }));
}

function valueStrings(rows, fields) {
  return uniqueSorted(valuesFor(rows, fields).flatMap((row) => softObjectPaths(row.value)));
}

function numericEntries(rows, fields) {
  return valuesFor(rows, fields).filter((row) =>
    typeof row.value === "number" || (row.value && typeof row.value === "object"));
}

function leafOrder(rootDocument) {
  const combined = propertyRows(rootDocument, "root").find((row) => row.name === "CombinedUpgrades");
  return softObjectPaths(combined?.value ?? []).map(packagePathFromObjectPath);
}

function sdkEvidence(paths) {
  const sdkRoot = paths.fsdTemplate;
  if (!sdkRoot || !fs.existsSync(sdkRoot)) return [];
  const candidates = [
    ["TriggerClusterHoldDuration", "AMicroMissileLauncher::TriggerClusterHoldDuration"],
    ["TriggerClusterActive", "AMicroMissileLauncher::TriggerClusterActive"],
    ["OnTriggerCluster", "AMicroMissileLauncher::OnTriggerCluster"],
    ["ReleaseCluster", "UClusterbombSpawner::ReleaseCluster"],
    ["Server_ReleaseCluster", "AMicroMissileLauncher::Server_ReleaseCluster"],
  ];
  const publicRoot = path.join(sdkRoot, "Source", "FSD", "Public");
  if (!fs.existsSync(publicRoot)) return [];
  const headers = [];
  const walk = (directory) => fs.readdirSync(directory, { withFileTypes: true }).forEach((entry) => {
    const filename = path.join(directory, entry.name);
    if (entry.isDirectory()) walk(filename);
    else if (entry.name.endsWith(".h")) headers.push(filename);
  });
  walk(publicRoot);
  const result = [];
  for (const filename of headers) {
    const text = fs.readFileSync(filename, "utf8");
    for (const [needle, symbol] of candidates) {
      if (text.includes(needle)) result.push({
        symbol,
        file: path.relative(analysisDir, filename).replaceAll("\\", "/"),
      });
    }
  }
  return result;
}

function documentClassNames(document) {
  return uniqueSorted([
    ...(document.Imports ?? []).map((item) => item.ObjectName),
    ...(document.Exports ?? []).flatMap((item) => [
      item.ObjectName,
      item.$type?.split(",")[0].split(".").at(-1),
    ]),
  ].filter(Boolean));
}

function classesHave(classNames, tokens) {
  return classNames.some((name) => tokens.some((token) =>
    name === token || name === `Default__${token}` || name.endsWith(`/${token}`)));
}

function classPaths(classNames, matcher) {
  return uniqueSorted(classNames.filter((name) => name.startsWith("/Game/") && matcher.test(name)));
}

function leafOperations(rows, operationPaths) {
  return operationPaths.map((packagePath) => {
    const packageRows = rows.filter((row) => row.packagePath === packagePath);
    const upgradeTypes = packageRows
      .filter((row) => row.name === "upgradeType" || row.name === "UpgradeType")
      .map((row) => row.value);
    const values = packageRows.filter((row) =>
      ["Value", "Amount", "Multiplier", "Add", "Scale", "ProjectileClass", "ComponentClass",
        "StatusEffect", "LifeSpan", "Lifetime", "Duration", "GrenadeFragments", "NumProjectiles",
        "ProjectileCount", "PelletCount", "TickInterval", "DamageInterval", "AoERange", "Radius"]
        .includes(row.name));
    return {
      packagePath,
      upgradeTypes,
      values,
    };
  }).filter((row) => row.upgradeTypes.length || row.values.length);
}

function buildMechanics(rows, dependencies, parsed, operationPaths) {
  // Mechanic flags must come from the overclock's actual CombinedUpgrades leaves.
  // Traversed dependencies include the weapon's ordinary projectile, explosion,
  // VFX trail and status implementation. Treating those transitive classes as
  // additions made numeric overclocks look as if they spawned persistent areas.
  const classNames = uniqueSorted(operationPaths.flatMap((packagePath) =>
    parsed.has(packagePath) ? documentClassNames(parsed.get(packagePath).document) : []));
  const operationRows = operationPaths.flatMap((packagePath) =>
    parsed.has(packagePath) ? propertyRows(parsed.get(packagePath).document, packagePath) : []);
  const switchProjectile = classesHave(classNames, CLASS_TOKENS.projectileReplacement);
  const addsComponent = classesHave(classNames, CLASS_TOKENS.componentAddition);
  const projectileReplacements = switchProjectile
    ? uniqueSorted([
        ...valueStrings(operationRows, FIELD_GROUPS.projectile),
        ...classPaths(classNames, /(?:\/PRJ_|\/Projectile|Projectile_)/i),
      ])
    : [];
  const components = valueStrings(operationRows, FIELD_GROUPS.component);
  const appliesStatus = classesHave(classNames, [
    "TriggeredStatusEffectUpgrade",
    "StickyFlameStatusEffectUpgrade",
  ]) || classNames.some((name) =>
    /(?:AddComponentStatusEffectItem|StatusEffectTrigger)/i.test(name));
  const statuses = appliesStatus ? valueStrings(operationRows, FIELD_GROUPS.status) : [];
  const inputs = valuesFor(operationRows, FIELD_GROUPS.input);
  const lifecycle = numericEntries(operationRows, FIELD_GROUPS.lifecycle);
  const spawns = numericEntries(operationRows, FIELD_GROUPS.spawn);
  const ticks = numericEntries(operationRows, FIELD_GROUPS.tick);
  const areas = numericEntries(operationRows, FIELD_GROUPS.area);
  const replication = valuesFor(operationRows, FIELD_GROUPS.replication);
  const writeTargets = uniqueSorted(operationRows
    .filter((row) => row.name === "upgradeType" || row.name === "UpgradeType")
    .map((row) => `${objectLeafName(row.packagePath)}:${JSON.stringify(row.value)}`));
  const persistentObjectSignal = classNames.some((name) =>
    /(?:Mine|Puddle|StickyIce|FlameWall|PersistentArea)/i.test(name));
  const persistentActor = persistentObjectSignal && (
    switchProjectile ||
    addsComponent ||
    classNames.some((name) => /(?:Spawner|SpawnActor)/i.test(name))
  );
  return {
    classSignals: classNames.filter((name) =>
      /(?:Upgrade|Component|Counter|Spawner|Projectile|StatusEffect|Mine|Puddle|Sticky|Trail|FlameWall)/i.test(name)),
    operations: leafOperations(rows, operationPaths),
    writeTargets,
    projectileReplacements,
    components: addsComponent ? uniqueSorted([
      ...components,
      ...classPaths(classNames, /Component/i),
    ]) : [],
    statuses: appliesStatus ? uniqueSorted([
      ...statuses,
      ...classPaths(classNames, /(?:\/STE_|StatusEffect)/i),
    ]) : [],
    inputs,
    lifecycle,
    spawns,
    ticks,
    areas,
    replication,
    projectileReplacement: switchProjectile,
    componentAddition: addsComponent,
    eventBinding: classesHave(classNames, CLASS_TOKENS.eventBinding),
    persistentActor,
    tickWork: ticks.length > 0 || (persistentActor &&
      classNames.some((name) => /(?:StatusEffectTrigger|DotStatusEffectItem|Mine|Puddle|StickyIce)/i.test(name))),
    areaWork: areas.length > 0 ||
      (persistentActor && classNames.some((name) => /(?:AoE|Mine|Puddle|StickyIce|FlameWall)/i.test(name))),
    statusPropagation: appliesStatus && (
      areas.length > 0 ||
      classNames.some((name) => /(?:StatusEffectTrigger|StickyFlameStatusEffectUpgrade)/i.test(name))
    ),
    childProjectileGeneration: classNames.some((name) =>
      /(?:ClusterbombSpawner|Fragment|Buckshot|MultiHitscan)/i.test(name)),
    dependencyCount: dependencies.length,
  };
}

function summaryZh(mechanics) {
  const parts = [];
  if (mechanics.projectileReplacement) parts.push("替换或改写投射物");
  if (mechanics.componentAddition) parts.push("向武器/投射物增加组件");
  if (mechanics.eventBinding) parts.push("注册命中或状态事件");
  if (mechanics.inputs.length) parts.push("包含特殊输入字段");
  if (mechanics.spawns.length) parts.push("生成子投射物或派生对象");
  else if (mechanics.childProjectileGeneration) parts.push("生成子投射物或多弹丸");
  if (mechanics.persistentActor) parts.push("生成有生命周期的持续对象");
  if (mechanics.statusPropagation) parts.push("施加或传播状态");
  if (!parts.length) parts.push("由叶级升级直接修改已确认字段");
  return `${parts.join("；")}。`;
}

function applyExactInputSignals(mechanics) {
  const signals = new Set(mechanics.classSignals);
  const add = (field, value, evidenceRef) => {
    if (!mechanics.inputs.some((row) => row.field === field)) {
      mechanics.inputs.push({ field, value, evidenceRef });
    }
  };
  if ([...signals].some((name) => name.endsWith("/UPC_Special_Return"))) {
    add("FireHoldRelease", {
      action: "hold-fire-then-release",
      state: "reverse-last-projectile",
    }, "sdk:ALineCutter::Server_ReverseLastProjectile");
  }
  if ([...signals].some((name) => name.endsWith("/UPC_Special_Spin"))) {
    add("FireHoldRelease", {
      action: "hold-fire-to-steer-release-to-stop",
      state: "rotate-last-projectile",
    }, "sdk:ALineCutter::Server_StopRotatingProjectile");
  }
  if (signals.has("CryoSprayProjectileUpgrade") ||
      signals.has("Default__CryoSprayProjectileUpgrade")) {
    add("PressurizedProjectile", {
      action: "special-projectile-charge-and-release",
      state: "cryospray-pressurized-projectile-slot",
    }, "sdk:ACryosprayItem::Server_PreLaunchProjectile");
  }
  if (mechanics.writeTargets.some((target) =>
    target.includes("EMicroMissileLauncherUpgrades::BuckFire"))) {
    add("FireHoldRelease", {
      action: "hold-fire-to-charge-release-to-salvo",
      state: "micro-missile-charge-mode",
    }, "sdk:AMicroMissileLauncher::OnStartCharging");
  }
}

export async function buildContracts() {
  const catalog = readJson(files.catalog);
  const parsedIndex = readJson(files.parsedIndex);
  const parsed = loadParsedIndex(parsedIndex);
  const paths = readJson(files.gamePaths);
  const sdk = sdkEvidence(paths);
  const overrides = readJson(files.implementationOverrides);
  const runtime = readJson(files.runtimeEvidence);
  const overclocks = [];

  for (const weapon of catalog.weapons) {
    for (const source of weapon.overclocks) {
      const root = packagePathFromObjectPath(source.objectPath);
      const dependencyPaths = resolveDependencyGraph(root, parsed);
      const rootRow = parsed.get(root);
      const orderedLeaves = rootRow ? leafOrder(rootRow.document) : [];
      const mechanicSeeds = uniqueSorted([root, ...orderedLeaves]);
      const mechanicPaths = uniqueSorted([
        ...mechanicSeeds,
        ...mechanicSeeds.flatMap((packagePath) => parsed.get(packagePath)?.references ?? []),
      ]).filter((packagePath) => parsed.has(packagePath));
      const leaves = orderedLeaves.map((packagePath, index) => ({
        index,
        packagePath,
        parsed: parsed.has(packagePath),
        classNames: uniqueSorted((parsed.get(packagePath)?.document?.Imports ?? [])
          .map((item) => item.ObjectName)
          .filter(Boolean)),
        properties: parsed.has(packagePath) ? propertyRows(parsed.get(packagePath).document, packagePath) : [],
      }));
      const allRows = mechanicPaths.flatMap((packagePath) =>
        parsed.has(packagePath) ? propertyRows(parsed.get(packagePath).document, packagePath) : []);
      const mechanics = buildMechanics(allRows, mechanicPaths, parsed, orderedLeaves);
      applyExactInputSignals(mechanics);
      const overclockId = stableId(source.objectPath);
      const manual = overrides.overclocks?.[overclockId] ?? overrides.overclocks?.[source.objectPath] ?? {};
      const runtimeClaims = (runtime.records ?? []).filter((record) => record.overclockId === overclockId);
      const claims = [{
        claimId: `${overclockId}:root`,
        claim: `根资产及 ${orderedLeaves.length} 个 CombinedUpgrades 叶项已展开。`,
        evidenceRefs: rootRow ? [`asset:${root}`, ...leaves.map((leaf) => `asset:${leaf.packagePath}`)] : [],
        confidence: rootRow && leaves.every((leaf) => leaf.parsed) ? "asset-confirmed" : "unresolved",
        reviewStatus: rootRow && leaves.every((leaf) => leaf.parsed) ? "machine-reviewed" : "needs-review",
      }, ...(manual.claims ?? []), ...runtimeClaims.map((record) => ({
        claimId: record.claimId,
        claim: record.claim,
        evidenceRefs: record.evidenceRefs,
        confidence: "runtime-confirmed",
        reviewStatus: "runtime-reviewed",
      }))];

      if (source.objectPath.includes("OC_MML_ClusterMissiles")) {
        claims.push({
          claimId: `${overclockId}:cluster-input`,
          claim: "集束释放由按住换弹触发；原生接口给出约 0.5 秒阈值，并作用于当前活动飞弹。",
          evidenceRefs: [
            `asset:${root}#CombinedUpgrades`,
            ...sdk.filter((row) => row.symbol.includes("Cluster")).map((row) => `sdk:${row.symbol}`),
          ],
          confidence: sdk.length ? "native-signature-only" : "asset-confirmed",
          reviewStatus: "evidence-reviewed",
        });
        mechanics.inputs.push({
          field: "SpecialReloadHold",
          value: { action: "reload-hold", thresholdSeconds: 0.5, target: "active-missiles" },
          evidenceRef: "sdk:AMicroMissileLauncher::TriggerClusterHoldDuration",
        });
        mechanics.eventBinding = true;
      }

      const criticalUnresolved = [];
      if (!rootRow) criticalUnresolved.push("root-asset");
      if (leaves.some((leaf) => !leaf.parsed)) criticalUnresolved.push("combined-upgrade-leaf");
      const mergedMechanics = { ...mechanics, ...(manual.mechanics ?? {}) };
      const dossier = {
        overclockId,
        classKey: weapon.classKey,
        className: weapon.className,
        slot: weapon.slot,
        weaponName: weapon.weaponName,
        assetFolder: weapon.assetFolder,
        nameZh: source.nameZh,
        nameEn: source.nameEn,
        objectPath: source.objectPath,
        rootAsset: root,
        rootSha256: rootRow?.uassetSha256 ?? null,
        dependencyGraph: dependencyPaths.map((packagePath) => ({
          packagePath,
          references: parsed.get(packagePath)?.references ?? [],
          sha256: parsed.get(packagePath)?.uassetSha256 ?? null,
        })),
        dependencies: dependencyPaths,
        combinedUpgradeOrder: orderedLeaves,
        leaves,
        mechanics: mergedMechanics,
        defaultBuild: source.defaultBuild,
        defaultBuildImpact: manual.defaultBuildImpact ?? [],
        summaryZh: manual.summaryZh ?? summaryZh(mergedMechanics),
        implementationHintZh: manual.implementationHintZh ??
          (mechanics.projectileReplacement || mechanics.inputs.length
            ? "先保留原超频入口，再对冲突叶项做桥接。"
            : "展开叶级升级后可按稳定顺序叠加。"),
        claims,
        gate: {
          status: criticalUnresolved.length ? "blocked" : "ready",
          criticalUnresolved,
        },
      };
      overclocks.push(dossier);
    }
  }

  overclocks.sort((a, b) => a.objectPath.localeCompare(b.objectPath, "en"));
  const gameBuild = readJson(files.gameBuild);
  const output = {
    schema: "dual-overclock-reward.overclock-implementations.v1",
    metadata: {
      targetGame: catalog.metadata.targetGame,
      pakSha256: gameBuild.pakSha256 ?? null,
      generatedAt: new Date().toISOString(),
      weaponCount: catalog.weapons.length,
      overclockCount: overclocks.length,
      readyCount: overclocks.filter((row) => row.gate.status === "ready").length,
      blockedCount: overclocks.filter((row) => row.gate.status === "blocked").length,
      scoringTextInference: false,
      disclaimer: "实现档案来自Cooked资产属性、Export、Kismet证据和SDK签名；不是官方源码还原。",
    },
    overclocks,
  };
  writeJson(files.implementations, output);
  writeJson(files.validationQueue, {
    schema: "dual-overclock-reward.runtime-validation-queue.v1",
    metadata: { generatedAt: output.metadata.generatedAt },
    records: overclocks.flatMap((row) => [
      ...row.gate.criticalUnresolved.map((field) => ({
        overclockId: row.overclockId,
        weaponName: row.weaponName,
        nameZh: row.nameZh,
        field,
        reason: "评分关键证据尚未完成。",
      })),
      ...(row.objectPath.includes("OC_MML_ClusterMissiles") ? [{
        overclockId: row.overclockId,
        weaponName: row.weaponName,
        nameZh: row.nameZh,
        field: "cluster-negative-triggers",
        reason: "实机回归：确认不存在命中、最后一发或距离自动释放；确认每枚活动飞弹仅释放一次并由服务器权威。",
      }] : []),
    ]),
  });

  const byWeapon = new Map();
  for (const row of overclocks) {
    if (!byWeapon.has(row.assetFolder)) byWeapon.set(row.assetFolder, []);
    byWeapon.get(row.assetFolder).push(row);
  }
  for (const [assetFolder, rows] of byWeapon) {
    const lines = [`# ${rows[0].weaponName} 超频实现档案`, "",
      "> 仅以Cooked资产与SDK证据描述实现，不使用攻略文字生成机制标签。", ""];
    for (const row of rows) {
      lines.push(`## ${row.nameZh}`, "", `- 资产：\`${row.objectPath}\``,
        `- 机制：${row.summaryZh}`, `- 叶级升级：${row.combinedUpgradeOrder.length} 项`,
        `- 证据门槛：${row.gate.status}`, `- 实现提示：${row.implementationHintZh}`, "");
    }
    writeText(path.join(generatedDir, "overclocks", `${assetFolder}.zh-CN.md`), `${lines.join("\n")}\n`);
  }
  return output;
}
