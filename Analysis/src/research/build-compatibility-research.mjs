import crypto from "node:crypto";
import path from "node:path";
import { files } from "../shared/paths.mjs";
import { readJson, writeJson, writeText } from "../shared/io.mjs";

const BUILD_TAG = "0.57.0-alpha93-shared-bank-init-probe";
const REGISTRATION_COUNTS = [2, 24, 100, 454];
const CLEAN_SYNC_FIELDS = new Set([
  "EAmmoDrivenWeapnUpgradeType::Damage",
  "EAmmoDrivenWeapnUpgradeType::AreaDamage",
  "EAmmoDrivenWeapnUpgradeType::AreaOfEffect",
  "EAmmoDrivenWeapnUpgradeType::MaxAmmo",
  "EAmmoDrivenWeapnUpgradeType::ClipSize",
  "EAmmoDrivenWeapnUpgradeType::RateOfFire",
  "EAmmoDrivenWeapnUpgradeType::ReloadSpeed",
  "EAmmoDrivenWeapnUpgradeType::ProjectileVelocity",
  "EAmmoDrivenWeapnUpgradeType::BaseSpread",
  "EAmmoDrivenWeapnUpgradeType::RecoilMultiplier",
  "EAmmoDrivenWeapnUpgradeType::HeatPerShot",
  "EAmmoDrivenWeapnUpgradeType::CoolingRate",
  "EAmmoDrivenWeapnUpgradeType::ChargeSpeed",
]);

function stableJson(value) {
  if (Array.isArray(value)) return `[${value.map(stableJson).join(",")}]`;
  if (value && typeof value === "object") {
    return `{${Object.keys(value).sort().map((key) =>
      `${JSON.stringify(key)}:${stableJson(value[key])}`).join(",")}}`;
  }
  return JSON.stringify(value);
}

function sha256(value) {
  return crypto.createHash("sha256").update(value).digest("hex");
}

function roundRobinPairs(pairs) {
  const priority = [385, 388]
    .map((testIndex) => pairs.find((row) => row.testIndex === testIndex))
    .filter(Boolean);
  const priorityIds = new Set(priority.map((row) => row.pairId));
  const groups = new Map();
  for (const pair of pairs) {
    if (priorityIds.has(pair.pairId)) continue;
    const list = groups.get(pair.weaponFolder) ?? [];
    list.push(pair);
    groups.set(pair.weaponFolder, list);
  }
  const orderedGroups = [...groups.entries()]
    .sort(([a], [b]) => a.localeCompare(b, "en"))
    .map(([, rows]) => rows.sort((a, b) => a.testIndex - b.testIndex));
  const result = [...priority];
  for (let depth = 0; result.length < pairs.length; depth += 1) {
    let added = false;
    for (const rows of orderedGroups) {
      if (rows[depth]) {
        result.push(rows[depth]);
        added = true;
      }
    }
    if (!added) break;
  }
  if (new Set(result.map((row) => row.pairId)).size !== pairs.length) {
    throw new Error("Registration tier ordering lost or duplicated pairs");
  }
  return result;
}

function buildRegistrationTiers(pairs) {
  const ordered = roundRobinPairs(pairs);
  return REGISTRATION_COUNTS.map((count) => {
    const selected = ordered.slice(0, count);
    return {
      count,
      strategy: count === 2
        ? "runtime-confirmed-vertical-pairs-385-and-388"
        : count === 454
          ? "all-supported-pairs"
          : "weapon-round-robin-after-385-and-388",
      pairTestIndices: selected.map((row) => row.testIndex),
      pairIds: selected.map((row) => row.pairId),
      pairObjectPaths: selected.map((row) => row.pairUpgradeObjectPath),
      selectionHash: sha256(stableJson(selected.map((row) => ({
        pairId: row.pairId,
        testIndex: row.testIndex,
        pairUpgradeObjectPath: row.pairUpgradeObjectPath,
      })))),
      executionStatus: count === 2
        ? "baseline-tier-contained-in-alpha55-replay-clear-authority-package"
        : "selection-ready-registration-pak-not-generated",
    };
  });
}

function buildCleanCandidates(runtime, implementations) {
  const implementationById = new Map(
    implementations.overclocks.map((row) => [row.overclockId, row]),
  );
  return runtime.overclocks
    .filter((row) => row.category === "clean")
    .map((row) => {
      const implementation = implementationById.get(row.overclockId);
      const mechanicFlags = implementation?.mechanics ?? {};
      const allAmmoNumericLeaves = implementation?.leaves?.length > 0 &&
        implementation.leaves.every((leaf) =>
          leaf.classNames.includes("AmmoDrivenWeaponUpgrade") &&
          leaf.classNames.every((name) =>
            name === "/Script/FSD" || name === "AmmoDrivenWeaponUpgrade" ||
            name === "Default__AmmoDrivenWeaponUpgrade"));
      const fields = (implementation?.mechanics?.operations ?? [])
        .flatMap((operation) => operation.upgradeTypes ?? []);
      const allFieldsKnown = fields.length === implementation?.leaves?.length &&
        fields.every((field) => CLEAN_SYNC_FIELDS.has(field));
      const noMechanics = [
        "projectileReplacement", "componentAddition", "eventBinding",
        "persistentActor", "tickWork", "areaWork", "statusPropagation",
        "childProjectileGeneration",
      ].every((key) => mechanicFlags[key] !== true) &&
        (mechanicFlags.inputs ?? []).length === 0;
      const candidate = allAmmoNumericLeaves && allFieldsKnown && noMechanics;
      return {
        cleanId: row.overclockId,
        nameZh: row.nameZh,
        weaponNameZh: row.weaponNameZh,
        weaponObjectPath: row.weaponObjectPath,
        slot: row.slot,
        rootObjectPath: row.objectPath,
        leafObjectPaths: row.leafObjectPaths,
        fields,
        candidate,
        status: candidate ? "unverified" : "excluded-static",
        exclusionReasons: [
          !allAmmoNumericLeaves ? "contains-non-ammo-numeric-leaf" : null,
          !allFieldsKnown ? "contains-unmapped-or-unsupported-field" : null,
          !noMechanics ? "contains-mechanic-signal" : null,
        ].filter(Boolean),
      };
    })
    .sort((a, b) =>
      Number(b.candidate) - Number(a.candidate) ||
      a.weaponNameZh.localeCompare(b.weaponNameZh, "zh-CN") ||
      a.nameZh.localeCompare(b.nameZh, "zh-CN"));
}

const COMMANDS = [
  ["!dor diag save <page>", "显示当前3枚注册基线和Alpha38恢复守卫；完整GUID数组审计尚未接入", "partial-read-only"],
  ["!dor diag net <page>", "显示玩家编号、自动握手、Proxy状态和最近一次客户端+服务器权威融合的合并结果", "available"],
  ["!dor save audit", "只读报告当前审计能力与2/24/100/452记录表位置；不修改存档", "partial-read-only"],
  ["!dor test pair <player#> <277|359|385|388|435|438|444>", "先由Owner客户端完成本地构造，再由服务器串行重建同一远端玩家的权威武器；双端均成功才回报成功", "available-manual-dual-machine-test-required"],
  ["!dor test reward <player#|all>", "模拟一次正式奖励轮；未安装玩家走原版100%库存补给；所有参与者选择及奖励处理结束后只请求一次播报潮", "available-manual-dual-machine-test-required"],
  ["!dor test hostpair <testIndex>", "仅向房主应用融合组合，供未安装客户端旁观", "available-277-359-385-388-435-438-444"],
  ["!dor test clean <player#> <cleanId>", "Clean白名单为空时明确拒绝；4枚静态候选仍需逐字段实测", "blocked-safe"],
  ["!dor test clear <player#>", "清除远端融合测试态；一次性补弹没有持续修正可清除，也不回收已补弹药", "available-manual-dual-machine-test-required"],
  ["!dor test reapply <player#>", "向兼容Owner客户端重发服务器保存的generation", "available-manual-dual-machine-test-required"],
  ["!dor test ammo <player#> all", "房主向目标库存调用原版Resupply(1.0)，补满可补给弹药；不增加上限、不重建武器、不依赖客户端DOR，待仅主机实测", "available-native-refill-runtime-test-required"],
  ["!dor test ammo <player#> primary|secondary|fallback", "兼容旧参数，全部等同all：一次性补满全库存弹药，不是单槽或最大弹药加成", "available-legacy-alias"],
  ["!dor diag ammo <player#>", "说明原版100%库存补给、客户端无需DOR和旧标签均等同all；不是实际弹药数值回读", "available"],
  ["!dor test classify <player#> installed|uninstalled|mismatch", "在内存中保存0..3号玩家的手工研究分类；不构成资产发送授权", "available"],
  ["!dor test netmode safe|clean|all-installed", "记录研究模式；Alpha45不据此绕过远端安全门或补启动奖励", "available-record-only"],
  ["!dor test handshake", "重新发送一次DOR安装识别挑战；同版本安装客户端自动应答", "available"],
];

function buildTestMatrix() {
  return [
    {
      id: "host-installed-client-uninstalled",
      defaultMode: "safe",
      expected: "no-proxy-no-ui-no-custom-asset-to-client",
      status: "manual-test-required",
    },
    {
      id: "host-uninstalled-client-installed",
      defaultMode: "dormant",
      expected: "no-master-no-proxy-no-reward-no-chat-no-weapon-mutation",
      status: "manual-test-required",
    },
    {
      id: "host-installed-client-installed",
      defaultMode: "all-installed",
      expected: "owner-client-and-server-authority-both-construct-fusion-and-confirm-version-hash",
      status: "alpha51-pair-runtime-confirmed-alpha55-replay-clear-runtime-test-required",
    },
    {
      id: "solo-installed",
      defaultMode: "safe",
      expected: "pairs-385-388-regression-pass-and-pair-277-manual-test",
      status: "pairs-385-388-confirmed-pair-277-manual-test-required",
    },
  ];
}

function csvCell(value) {
  const text = String(value ?? "");
  return /[",\r\n]/.test(text) ? `"${text.replaceAll('"', '""')}"` : text;
}

function buildCleanCsv(rows) {
  const header = [
    "cleanId", "武器", "槽位", "Clean超频", "静态候选", "状态", "字段", "排除原因",
    "主机数值", "拥有者客户端数值", "HUD一致", "开火节奏一致", "弹药一致", "结论", "备注",
  ];
  const body = rows.map((row) => [
    row.cleanId, row.weaponNameZh, row.slot, row.nameZh, row.candidate ? "是" : "否",
    row.status, row.fields.join("|"), row.exclusionReasons.join("|"), "", "", "", "", "", "", "",
  ].map(csvCell).join(","));
  return `${header.map(csvCell).join(",")}\n${body.join("\n")}\n`;
}

function buildScenarioCsv(rows) {
  const header = [
    "场景", "默认模式", "测试项", "房主结果", "客户端结果", "崩溃/断线", "自定义资产泄漏", "结论", "备注",
  ];
  const body = rows.map((row) => [
    row.id, row.defaultMode, row.expected, "", "", "", "", "", "",
  ].map(csvCell).join(","));
  return `${header.map(csvCell).join(",")}\n${body.join("\n")}\n`;
}

function buildSaveScaleCsv(tiers) {
  const header = [
    "融合数量", "选择策略", "选择哈希", "执行状态", "购买数组", "拥有数组", "已锻造数组", "重复项", "孤立GUID",
    "存档字节", "读取ms", "保存ms", "空间站载入ms", "装备终端ms", "锻造台ms", "进任务ms", "返空间站ms", "结论", "备注",
  ];
  const body = tiers.map((tier) => [
    tier.count, tier.strategy, tier.selectionHash, tier.executionStatus,
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
  ].map(csvCell).join(","));
  return `${header.map(csvCell).join(",")}\n${body.join("\n")}\n`;
}

function buildSaveLifecycleCsv() {
  const header = [
    "注册规模", "阶段", "DOR版本", "游戏版本", "购买数组", "拥有数组", "已锻造数组",
    "融合装备引用", "孤立GUID", "存档字节", "可进空间站", "可加入房间", "可正常游玩",
    "锻造/矿心/精通正常", "Fixup异常", "结论", "备注",
  ];
  const stages = [
    "安装并注册", "重启游戏", "异常退出后重启", "卸载后", "重装同版本",
    "更新DOR版本", "删除或调整组合后", "游戏版本更新后", "云存档往返后",
  ];
  const body = REGISTRATION_COUNTS.flatMap((count) => stages.map((stage) => [
    count, stage, "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
  ].map(csvCell).join(",")));
  return `${header.map(csvCell).join(",")}\n${body.join("\n")}\n`;
}

function buildNetworkAssetCsv(rows) {
  const header = [
    "资产/机制", "当前复制策略", "允许未知客户端", "需要安装确认", "旁观测试",
    "晚加入", "复活/重建", "客户端崩溃", "断线/软锁", "显示异常", "结论", "备注",
  ];
  const body = rows.map((row) => [
    row.asset, row.replication, row.unknownPeerAllowed ? "是" : "否",
    row.unknownPeerAllowed ? "否" : "是", "", "", "", "", "", "", "", "",
  ].map(csvCell).join(","));
  return `${header.map(csvCell).join(",")}\n${body.join("\n")}\n`;
}

function buildAllInstalledCsv() {
  const header = [
    "测试类别", "样例", "房主", "拥有者客户端", "旁观客户端", "Actor一致", "融合根一致",
    "叶顺序一致", "弹药一致", "输入一致", "状态/投射物一致", "生命周期", "结论", "备注",
  ];
  const rows = [
    ["数值融合", "385/388"],
    ["触发式超频", "高热/满盾/过热事件"],
    ["长按/松开输入", "集束、雪球/冰锥等"],
    ["强制放电", "对应输入状态机"],
    ["状态组件", "Debuff与切枪后生命周期"],
    ["子投射物", "齐射/集束/弹射"],
    ["持续区域", "泥潭/火焰/冰棘/地雷"],
    ["补给", "弹药上限和HUD"],
    ["死亡复活", "任务态重放"],
    ["晚加入/重连", "版本、哈希、generation"],
  ];
  const body = rows.map(([category, sample]) => [
    category, sample, "", "", "", "", "", "", "", "", "", "", "", "",
  ].map(csvCell).join(","));
  return `${header.map(csvCell).join(",")}\n${body.join("\n")}\n`;
}

function buildReadme(manifest) {
  const candidateCount = manifest.cleanCandidates.filter((row) => row.candidate).length;
  return `# DOR 存档与联机兼容研究包\n\n` +
    `构建：\`${manifest.buildTag}\`  \n` +
    `运行数据哈希：\`${manifest.sourceManifestHash}\`\n\n` +
    `本目录是研究记录，不是已经通过的兼容性声明。Alpha51在客户端事务完成后，串行执行服务器权威武器重建；只有双端都成功才回报最终成功；` +
    `未知玩家一律按未安装处理。初步静态筛出 ${candidateCount} 枚纯数值Clean候选，` +
    `但Alpha56最大弹药实验已实测失败。Alpha57改用原版Inventory.Resupply(1.0)一次性补弹，不再增加上限，仍需仅主机双机实测。\n\n` +
    `- \`save-registration-scale.csv\`：2/24/100/452注册规模记录。\n` +
    `- \`save-lifecycle.csv\`：卸载、重装、升级、孤立GUID和云存档生命周期记录。\n` +
    `- \`multiplayer-scenarios.csv\`：四种安装场景记录。\n` +
    `- \`network-asset-dependencies.csv\`：DOR资产网络依赖与混装停止门。\n` +
    `- \`clean-sync-matrix.csv\`：Clean字段同步验证。\n` +
    `- \`all-installed-sync.csv\`：全员安装下机制、输入、弹药与生命周期同步验证。\n` +
    `- \`research-commands.zh-CN.md\`：研究指令与安全限制。\n`;
}

export function buildCompatibilityResearch(outputPaths = {}) {
  const runtimeManifestPath = outputPaths.runtimeManifest ?? files.runtimeManifest;
  const compatibilityResearchManifestPath =
    outputPaths.compatibilityResearchManifest ?? files.compatibilityResearchManifest;
  const outputDir = outputPaths.outputDir ??
    outputPaths.compatibilityResearchDir ?? files.compatibilityResearchDir;
  const runtime = readJson(runtimeManifestPath);
  const implementations = readJson(files.implementations);
  if (runtime.pairs.length !== 454 || runtime.overclocks.length !== 160) {
    throw new Error("Compatibility research requires the complete 454/160 runtime manifest");
  }
  const registrationTiers = buildRegistrationTiers(runtime.pairs);
  const cleanCandidates = buildCleanCandidates(runtime, implementations);
  const testMatrix = buildTestMatrix();
  const manifest = {
    schema: "dual-overclock-reward.compatibility-research.v1",
    sourceManifestHash: runtime.manifestHash,
    buildTag: BUILD_TAG,
    safetyDefaults: {
      networkMode: "all-installed-with-native-refill-fallback",
      unknownPeerClassification: "uninstalled",
      versionOrHashMismatch: "mismatch",
      masterReplication: false,
      proxyCreation: "installed-compatible-only",
      sendCustomAssetsBeforeConfirmation: false,
      cleanWhitelistDefault: "deny",
      saveAuditMutatesSave: false,
    },
    knownFacts: {
      uninstallCanLoadAndPlay: true,
      installedClientIntoVanillaHostTested: false,
      pairs385And388SoloEffectsConfirmed: true,
      externalPresetRestoredAfterMission: true,
      hundredsOfFusionGuidsLongTermSafetyKnown: false,
    },
    deployedRegistration: {
      count: 7,
      pairTestIndices: [277, 359, 385, 388, 435, 438, 444],
      status: "alpha93-shared-bank-init-probe-local-build-and-runtime-test-required",
    },
    registrationTiers,
    cleanCandidates,
    networkAssetPolicy: [
      { asset: "BP_DOR_Master", replication: "host-local-only", unknownPeerAllowed: false },
      { asset: "BP_DOR_PlayerProxy", replication: "owner-only-after-compatible-classification", unknownPeerAllowed: false },
      { asset: "/Game/DualOverclockReward/Pairs/*", replication: "installed-compatible-only", unknownPeerAllowed: false },
      { asset: "original-game-projectiles-and-status", replication: "spectator-test-required", unknownPeerAllowed: true },
      { asset: "DOR-bridge-projectiles-components", replication: "mixed-client-stop-gate", unknownPeerAllowed: false },
    ],
    installDetection: {
      localUGCRegistry: "researchable-local-only",
      currentSessionPerPeerList: "unresolved",
      automaticHandshake: "confirmed-over-original-chat-channel-on-user-dual-machine-session",
      manualResearchOverride: true,
    },
    commands: COMMANDS.map(([command, purpose, runtimeAvailability]) => ({
      command,
      purpose,
      runtimeAvailability,
    })),
    testMatrix,
    stopGates: [
      "registration-affects-forging-overclock-rewards-save-load-or-external-presets",
      "uninstalled-client-crash-disconnect-softlock-or-custom-asset-load",
      "clean-field-host-owner-desync",
      "installed-client-activates-dor-when-host-is-vanilla",
      "all-installed-special-mechanics-or-ammo-only-correct-on-host",
    ],
  };

  writeJson(compatibilityResearchManifestPath, manifest);
  writeText(path.join(outputDir, "README.zh-CN.md"), buildReadme(manifest));
  writeText(path.join(outputDir, "save-registration-scale.csv"), buildSaveScaleCsv(registrationTiers));
  writeText(path.join(outputDir, "save-lifecycle.csv"), buildSaveLifecycleCsv());
  writeText(path.join(outputDir, "multiplayer-scenarios.csv"), buildScenarioCsv(testMatrix));
  writeText(
    path.join(outputDir, "network-asset-dependencies.csv"),
    buildNetworkAssetCsv(manifest.networkAssetPolicy),
  );
  writeText(path.join(outputDir, "clean-sync-matrix.csv"), buildCleanCsv(cleanCandidates));
  writeText(path.join(outputDir, "all-installed-sync.csv"), buildAllInstalledCsv());
  writeText(
    path.join(outputDir, "research-commands.zh-CN.md"),
    `# 研究指令\n\n${COMMANDS.map(([command, purpose, status]) => `- \`${command}\`（${status}）：${purpose}。`).join("\n")}\n\n` +
    `所有玩家编号以 \`!dor diag net 1\` 当前显示为准。未知、超时、版本或哈希不一致均不得创建Proxy或发送DOR资产。\n`,
  );
  return {
    metadata: {
      buildTag: BUILD_TAG,
      registrationTiers: registrationTiers.map((row) => row.count),
      cleanCandidateCount: cleanCandidates.filter((row) => row.candidate).length,
      cleanExcludedCount: cleanCandidates.filter((row) => !row.candidate).length,
    },
  };
}
