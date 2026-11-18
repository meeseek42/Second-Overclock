import path from "node:path";
import { files, generatedDir } from "../shared/paths.mjs";
import { readJson, writeText } from "../shared/io.mjs";

function csvCell(value) {
  const text = value === null || value === undefined ? "" :
    typeof value === "object" ? JSON.stringify(value) : String(value);
  return `"${text.replaceAll('"', '""')}"`;
}

function markdownTable(rows) {
  const lines = [
    "| 武器 | 超频A | 超频B | 分数 | 原因 |",
    "|---|---|---|---|---|",
  ];
  for (const row of rows) {
    lines.push(`| ${row.weaponName} | ${row.ocA.nameZh} | ${row.ocB.nameZh} | ${row.score} | ${row.reason || "—"} |`);
  }
  return lines.join("\n");
}

function assessmentRows(data) {
  return data.assessments.map((row) => ({
    武器: row.weaponName,
    超频A: row.ocA.nameZh,
    超频B: row.ocB.nameZh,
    原版机制A: row.ocA.mechanismZh,
    原版机制B: row.ocB.mechanismZh,
    精确冲突: row.collision.explanationZh,
    Bug原因: row.reasons.bugZh,
    性能原因: row.reasons.performanceZh,
    强度友伤原因: row.reasons.strengthFriendlyFireZh,
    推荐玩法名称: row.recommendation.nameZh,
    推荐趣味改法: row.recommendation.funChangeZh,
    必要保护边界: row.recommendation.protectionBoundaryZh,
    实现复杂度: row.scores.implementationComplexity,
    机制冲突度: row.scores.mechanicConflict,
    破坏性强度: row.scores.destructivePower,
    性能风险: row.scores.performanceRisk,
    Bug风险: row.scores.bugRisk,
    可行性: row.feasibility,
    路线: row.route,
    需要决定: row.ownerDecisionRequired ? "是" : "否",
    证据等级: row.evidence.highestConfidence,
    疑点: row.evidence.unresolved.join("；"),
    我的意见: "",
    审查状态: "",
  }));
}

export async function buildReports() {
  const contracts = readJson(files.implementations);
  const pairs = readJson(files.pairAssessments);
  const rows = assessmentRows(pairs);
  const headers = Object.keys(rows[0]);
  writeText(path.join(generatedDir, "combinations", "combination-matrix.csv"),
    `\uFEFF${[headers.map(csvCell), ...rows.map((row) => headers.map((header) => csvCell(row[header])))].map((row) => row.join(",")).join("\n")}\n`);

  const byWeapon = new Map();
  for (const row of pairs.assessments) {
    if (!byWeapon.has(row.assetFolder)) byWeapon.set(row.assetFolder, []);
    byWeapon.get(row.assetFolder).push(row);
  }
  for (const [assetFolder, weaponRows] of byWeapon) {
    const lines = [`# ${weaponRows[0].weaponName} 双超频组合`, "",
      "| 超频A | 超频B | 复杂度 | 冲突 | 强度 | 性能 | Bug | 可行性 | 路线 |",
      "|---|---|---:|---:|---:|---:|---:|---|---|"];
    for (const row of weaponRows) {
      lines.push(`| ${row.ocA.nameZh} | ${row.ocB.nameZh} | ${row.scores.implementationComplexity} | ${row.scores.mechanicConflict} | ${row.scores.destructivePower} | ${row.scores.performanceRisk} | ${row.scores.bugRisk} | ${row.feasibility} | ${row.route} |`);
    }
    writeText(path.join(generatedDir, "combinations", "weapons", `${assetFolder}.zh-CN.md`), `${lines.join("\n")}\n`);
  }

  const watchlists = [
    ["performance-watchlist.zh-CN.md", "性能关注清单", "performanceRisk", "performanceZh", 3],
    ["bug-watchlist.zh-CN.md", "Bug关注清单", "bugRisk", "bugZh", 3],
    ["conflict-watchlist.zh-CN.md", "机制冲突关注清单", "mechanicConflict", null, 4],
  ];
  for (const [filename, title, scoreKey, reasonKey, threshold] of watchlists) {
    const selected = pairs.assessments.filter((row) => row.scores[scoreKey] >= threshold).map((row) => ({
      ...row,
      score: row.scores[scoreKey],
      reason: reasonKey ? row.reasons[reasonKey] : row.collision.explanationZh,
    }));
    writeText(path.join(generatedDir, "watchlists", filename),
      `# ${title}\n\n> 无具体原因的组合不会因“机制复杂”被强行列入。\n\n${markdownTable(selected)}\n`);
  }
  const owner = pairs.assessments.filter((row) => row.ownerDecisionRequired);
  writeText(path.join(generatedDir, "watchlists", "owner-decisions.zh-CN.md"),
    `# 需要你决定的组合\n\n共 ${owner.length} 项；具体玩法由“推荐趣味改法”给出候选，不替你定案。\n\n${owner.map((row) =>
      `## ${row.weaponName}：${row.ocA.nameZh} + ${row.ocB.nameZh}\n\n- 冲突：${row.collision.explanationZh || "无直接冲突"}\n- 推荐：${row.recommendation.funChangeZh}\n- 保护：${row.recommendation.protectionBoundaryZh}\n`).join("\n")}`);

  const doubts = readJson(files.validationQueue);
  const doubtRows = doubts.records.map((row) => ({
    weaponName: row.weaponName,
    ocA: { nameZh: row.nameZh },
    ocB: { nameZh: row.field },
    score: "",
    reason: row.reason,
  }));
  writeText(path.join(generatedDir, "overclocks", "单超频疑点核对表.zh-CN.md"),
    `# 单超频疑点核对表\n\n${markdownTable(doubtRows)}\n`);
  return {
    metadata: {
      overclockCount: contracts.overclocks.length,
      pairCount: pairs.assessments.length,
      ownerDecisionCount: owner.length,
    },
  };
}
