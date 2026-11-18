import test from "node:test";
import assert from "node:assert/strict";
import { files } from "../../src/shared/paths.mjs";
import { readJson } from "../../src/shared/io.mjs";

test("catalog has the fixed game inventory", () => {
  const catalog = readJson(files.catalog);
  assert.equal(catalog.weapons.length, 24);
  assert.equal(catalog.weapons.flatMap((row) => row.overclocks).length, 160);
});

test("derived contracts and pairs are complete when present", () => {
  const contracts = readJson(files.implementations);
  const pairs = readJson(files.pairAssessments);
  assert.equal(contracts.overclocks.length, 160);
  assert.equal(new Set(contracts.overclocks.map((row) => row.overclockId)).size, 160);
  assert.equal(pairs.assessments.length, 461);
  assert.equal(new Set(pairs.assessments.map((row) => row.pairId)).size, 461);
  for (const row of pairs.assessments) {
    assert.equal(row.myOpinion, "");
    if (row.scores.bugRisk === 0) assert.equal(row.reasons.bugZh, "");
    if (row.scores.performanceRisk === 0) assert.equal(row.reasons.performanceZh, "");
  }
});

test("cluster charges regression uses reload hold", () => {
  const contracts = readJson(files.implementations);
  const cluster = contracts.overclocks.find((row) => row.objectPath.includes("OC_MML_ClusterMissiles"));
  assert.ok(cluster);
  const input = cluster.mechanics.inputs.find((row) => row.field === "SpecialReloadHold");
  assert.deepEqual(input.value, {
    action: "reload-hold",
    thresholdSeconds: 0.5,
    target: "active-missiles",
  });
});
