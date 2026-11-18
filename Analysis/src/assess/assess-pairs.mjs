import { files } from "../shared/paths.mjs";
import { readJson, stableId, uniqueSorted, writeJson } from "../shared/io.mjs";

function intersect(a = [], b = []) {
  const right = new Set(b.map((value) => JSON.stringify(value)));
  return a.filter((value) => right.has(JSON.stringify(value)));
}

function clamp(value) {
  return Math.max(0, Math.min(5, Math.round(value)));
}

function specialCount(oc) {
  const m = oc.mechanics;
  return [
    m.projectileReplacement,
    m.componentAddition,
    m.eventBinding,
    m.persistentActor,
    m.tickWork,
    m.areaWork,
    m.statusPropagation,
    m.inputs?.length > 0,
    m.spawns?.length > 0,
  ].filter(Boolean).length;
}

function assessment(ocA, ocB, manual = {}) {
  const a = ocA.mechanics;
  const b = ocB.mechanics;
  const sharedWrites = intersect(a.writeTargets, b.writeTargets);
  const sharedProjectiles = intersect(a.projectileReplacements, b.projectileReplacements);
  const sharedComponents = intersect(a.components, b.components);
  const sharedInputs = intersect(a.inputs?.map((row) => row.field), b.inputs?.map((row) => row.field));
  const sharedStatuses = intersect(a.statuses, b.statuses);
  const bothProjectileReplacement = a.projectileReplacement && b.projectileReplacement;
  const bothEventBinding = a.eventBinding && b.eventBinding;
  const spawnA = (a.spawns?.length ?? 0) + (a.childProjectileGeneration ? 1 : 0);
  const spawnB = (b.spawns?.length ?? 0) + (b.childProjectileGeneration ? 1 : 0);
  const persistentA = a.persistentActor ? 1 : 0;
  const persistentB = b.persistentActor ? 1 : 0;
  const areaA = a.areaWork ? 1 : 0;
  const areaB = b.areaWork ? 1 : 0;
  const statusA = a.statusPropagation ? 1 : 0;
  const statusB = b.statusPropagation ? 1 : 0;

  let conflict = 0;
  if (sharedWrites.length) conflict = Math.max(conflict, 2);
  if (bothEventBinding) conflict = Math.max(conflict, 3);
  if (sharedInputs.length) conflict = Math.max(conflict, 3);
  if (sharedProjectiles.length || bothProjectileReplacement) conflict = Math.max(conflict, 4);
  if (sharedComponents.length) conflict = Math.max(conflict, 4);

  let complexity = conflict === 0 ? 0 : conflict === 1 ? 1 : conflict === 2 ? 2 : conflict === 3 ? 3 : 4;
  if (bothProjectileReplacement && sharedInputs.length) complexity = 5;

  let performance = 0;
  if ((spawnA && spawnB) || ((spawnA || spawnB) && (persistentA || persistentB))) performance += 2;
  if ((persistentA && persistentB) || ((areaA + areaB) >= 2 && (statusA + statusB) >= 1)) performance += 2;
  if ((a.tickWork && b.tickWork) || ((a.tickWork || b.tickWork) && (spawnA + spawnB >= 2))) performance += 1;
  performance = clamp(performance);

  let bug = 0;
  if (sharedInputs.length) bug += 2;
  if (sharedProjectiles.length || bothProjectileReplacement) bug += 2;
  if (sharedComponents.length || bothEventBinding) bug += 1;
  if ((a.persistentActor || b.persistentActor) && (sharedComponents.length || bothEventBinding)) bug += 1;
  bug = clamp(bug);

  let power = 1;
  const specials = specialCount(ocA) + specialCount(ocB);
  if (specials >= 2) power++;
  if ((areaA + areaB) && (statusA + statusB)) power++;
  if ((spawnA + spawnB) >= 2) power++;
  if (bothProjectileReplacement && sharedProjectiles.length) power++;
  power = clamp(power);

  const ownerDecisionRequired = bothProjectileReplacement || sharedInputs.length > 0 ||
    (conflict >= 4 && (bothEventBinding || sharedComponents.length > 0));
  let feasibility = "可直接实现";
  let route = "直接叠加";
  if (conflict === 2) {
    feasibility = "需要保护";
    route = "顺序保护";
  } else if (conflict === 3) {
    feasibility = "需要桥接";
    route = "轻量桥接";
  } else if (ownerDecisionRequired) {
    feasibility = "等待决定";
    route = "定制融合";
  } else if (conflict >= 4) {
    feasibility = "需要定制融合";
    route = "定制融合";
  }

  const collisionParts = [];
  if (sharedWrites.length) collisionParts.push(`双方写入同一升级目标：${sharedWrites.join("、")}`);
  if (bothProjectileReplacement) collisionParts.push("双方均替换或改写同一武器的投射物链");
  if (sharedInputs.length) collisionParts.push(`双方争用输入入口：${sharedInputs.join("、")}`);
  if (bothEventBinding) collisionParts.push("双方均注册命中/状态事件，需阻止重复回调");
  if (sharedComponents.length) collisionParts.push(`双方增加同一组件：${sharedComponents.join("、")}`);
  const conflictReason = collisionParts.join("；");

  const bugReasons = [];
  if (sharedInputs.length) bugReasons.push("同一手势可能同时改变两个状态，需明确输入优先级和重入条件");
  if (bothProjectileReplacement) bugReasons.push("后应用的投射物替换可能覆盖先应用者，或产生客户端类不同步");
  if (bothEventBinding) bugReasons.push("同一生命周期事件可能重复注册并重复结算");
  if (sharedComponents.length) bugReasons.push("重复组件可能造成多次回调或清理遗漏");

  const perfReasons = [];
  if (spawnA && spawnB) perfReasons.push("母弹与子投射物数量形成乘法");
  if ((spawnA || spawnB) && (persistentA || persistentB)) perfReasons.push("每个派生投射物都可能留下持续对象");
  if (persistentA && persistentB) perfReasons.push("两类持续对象同时累积，四人高虫量场景可能难以稳定60 FPS");
  if (a.tickWork && b.tickWork) perfReasons.push("双方持续Tick/范围查询叠加");

  const strengthReasons = [];
  if ((areaA + areaB) && (statusA + statusB)) strengthReasons.push("范围覆盖与状态传播叠加，清群上限明显提高");
  if (spawnA && spawnB) strengthReasons.push("单发派生对象数量增加，伤害与友伤覆盖同步放大");
  if (power >= 4) strengthReasons.push("双方特殊机制可在同一发或同一目标上连续结算");

  let funName = `${ocA.nameZh} × ${ocB.nameZh}`;
  let funChange = "完整保留两枚超频各自的触发、重复命中和视觉识别度，由原版机制分别结算；只阻止同一个事件处理器被意外重复注册。";
  let protection = "伤害与状态由服务器权威结算；切枪或倒地只解除当前输入、控制引用和临时监听，已生成的投射物、持续区域及目标状态按原版生命周期继续；补给不清理世界效果。";
  if (bothProjectileReplacement) {
    funName = "双核弹头";
    funChange = "制作一条融合投射物链：以更有节目效果的一方为母弹，另一方作为命中、手动触发或销毁阶段的第二幕；不把任一方退化成纯数值。";
    protection = "禁止递归生成；母弹和子弹分别设置一次性标记、数量上限、寿命与服务器回收。";
  } else if (sharedInputs.length) {
    funName = "一键双态";
    funChange = "同一手势按阶段触发：按住期间保留蓄力/操控，达到阈值触发第二机制，松开只负责收尾；如改变原触发必须标为创意改动。";
    protection = "明确按下、达到阈值、松开三态；禁止一次输入重复进入；切枪或倒地只复位输入状态和控制引用，已经生成的投射物与状态继续按自身生命周期运行。";
  } else if (spawnA && spawnB) {
    funName = "俄罗斯套弹";
    funChange = "首版原样保留双方母弹、子投射物、飞行、碰撞和持续物流程，让数量乘法自然形成节目效果；对象上限只在实测确认失控后加入。";
    protection = "服务器记录每次生成链和活动对象数量；只禁止递归生成及同一释放事件的意外重复注册，不预先削减合法子投射物或持续物。";
  } else if (persistentA && persistentB) {
    funName = "禁区调色盘";
    funChange = "首版让两类持续区域按各自原版生成、结算和到期，不因组合而自动合并或提前回收；确认确有冲突后再设计交叠互动。";
    protection = "各持续物独立计时和销毁；切枪、补给或倒地不清除已生成区域；只清除失效引用并记录活动对象数量。";
  }

  const evidence = uniqueSorted([
    ...ocA.claims.flatMap((claim) => claim.evidenceRefs),
    ...ocB.claims.flatMap((claim) => claim.evidenceRefs),
  ]);
  const unresolved = uniqueSorted([
    ...ocA.gate.criticalUnresolved.map((item) => `A:${item}`),
    ...ocB.gate.criticalUnresolved.map((item) => `B:${item}`),
  ]);
  const result = {
    pairId: stableId([ocA.overclockId, ocB.overclockId].sort().join("+")),
    className: ocA.className,
    slot: ocA.slot,
    weaponName: ocA.weaponName,
    assetFolder: ocA.assetFolder,
    ocA: {
      overclockId: ocA.overclockId,
      nameZh: ocA.nameZh,
      nameEn: ocA.nameEn,
      objectPath: ocA.objectPath,
      mechanismZh: ocA.summaryZh,
    },
    ocB: {
      overclockId: ocB.overclockId,
      nameZh: ocB.nameZh,
      nameEn: ocB.nameEn,
      objectPath: ocB.objectPath,
      mechanismZh: ocB.summaryZh,
    },
    collision: {
      sharedWrites,
      sharedProjectiles,
      sharedComponents,
      sharedInputs,
      sharedStatuses,
      explanationZh: conflictReason,
    },
    reasons: {
      bugZh: bugReasons.join("；"),
      performanceZh: perfReasons.join("；"),
      strengthFriendlyFireZh: strengthReasons.join("；"),
    },
    recommendation: {
      nameZh: funName,
      funChangeZh: funChange,
      protectionBoundaryZh: protection,
      creativeTriggerChange: sharedInputs.length > 0,
    },
    scores: {
      implementationComplexity: complexity,
      mechanicConflict: conflict,
      destructivePower: power,
      performanceRisk: performance,
      bugRisk: bug,
    },
    feasibility,
    route,
    ownerDecisionRequired,
    evidence: {
      refs: evidence,
      highestConfidence: unresolved.length ? "unresolved" : "asset-confirmed",
      unresolved,
    },
    reviewStatus: "待人工审查",
    myOpinion: "",
  };
  Object.assign(result, manual);
  result.collision = { ...result.collision, ...(manual.collision ?? {}) };
  result.reasons = { ...result.reasons, ...(manual.reasons ?? {}) };
  result.recommendation = { ...result.recommendation, ...(manual.recommendation ?? {}) };
  result.scores = { ...result.scores, ...(manual.scores ?? {}) };
  result.evidence = { ...result.evidence, ...(manual.evidence ?? {}) };
  result.myOpinion = "";
  return result;
}

export function assessPairs() {
  const contracts = readJson(files.implementations);
  const blocked = contracts.overclocks.filter((row) => row.gate.status !== "ready");
  if (blocked.length) {
    throw new Error(
      `Assessment evidence gate blocked ${blocked.length} overclock(s): ` +
      blocked.map((row) => `${row.nameZh}[${row.gate.criticalUnresolved.join(",")}]`).join("; "),
    );
  }
  const manual = readJson(files.pairOverrides);
  const byWeapon = new Map();
  for (const oc of contracts.overclocks) {
    if (!byWeapon.has(oc.assetFolder)) byWeapon.set(oc.assetFolder, []);
    byWeapon.get(oc.assetFolder).push(oc);
  }
  const assessments = [];
  for (const overclocks of byWeapon.values()) {
    for (let i = 0; i < overclocks.length; i++) {
      for (let j = i + 1; j < overclocks.length; j++) {
        const pairId = stableId([overclocks[i].overclockId, overclocks[j].overclockId].sort().join("+"));
        assessments.push(assessment(overclocks[i], overclocks[j], manual.pairs?.[pairId] ?? {}));
      }
    }
  }
  assessments.sort((a, b) =>
    a.assetFolder.localeCompare(b.assetFolder, "en") ||
    a.ocA.nameZh.localeCompare(b.ocA.nameZh, "zh-CN") ||
    a.ocB.nameZh.localeCompare(b.ocB.nameZh, "zh-CN"));
  const output = {
    schema: "dual-overclock-reward.pair-assessments.v2",
    metadata: {
      generatedAt: new Date().toISOString(),
      pairCount: assessments.length,
      weaponCount: byWeapon.size,
      ownerDecisionCount: assessments.filter((row) => row.ownerDecisionRequired).length,
      scoringTextInference: false,
      performanceAudience: "面向接受高难、加怪与娱乐模组负载的玩家；4分才表示高压场景很可能无法稳定60 FPS。",
    },
    assessments,
  };
  writeJson(files.pairAssessments, output);
  return output;
}
