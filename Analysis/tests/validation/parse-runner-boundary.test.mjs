import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import test from "node:test";
import { runInspector } from "../../src/parse/parse-assets.mjs";

const fixtureRoot = fileURLToPath(new URL(
  "../../research/phase-review-20261008/unit5-current-suite-maintenance/temp/",
  import.meta.url,
));

function makeMockInspector(mode) {
  fs.mkdirSync(fixtureRoot, { recursive: true });
  const scriptPath = path.join(fixtureRoot, `mock-inspector-${mode}-${Date.now()}-${Math.random()}.mjs`);
  fs.writeFileSync(scriptPath, `
import fs from "node:fs";
import path from "node:path";
const listPath = process.argv[2];
const outputDirectory = process.argv[3];
const inputs = fs.readFileSync(listPath, "utf8").trim().split(/\\r?\\n/).filter(Boolean);
if (${JSON.stringify(mode)} !== "missing") {
  const results = ${JSON.stringify(mode)} === "duplicate"
    ? [{ input: inputs[0], success: true }, { input: inputs[0], success: true }]
    : ${JSON.stringify(mode)} === "mismatch"
      ? [{ input: path.resolve(outputDirectory, "not-requested.uasset"), success: true }, { input: inputs[1], success: true }]
    : inputs.map((input) => ({ input, success: true }));
  fs.writeFileSync(path.join(outputDirectory, "batch-result.json"), JSON.stringify({ results }));
}
if (${JSON.stringify(mode)} === "fail") process.exit(7);
`, "utf8");
  return scriptPath;
}

function runMock(mode) {
  const outputDirectory = fs.mkdtempSync(path.join(fixtureRoot, `parse-${mode}-`));
  const inputs = [
    path.resolve(fixtureRoot, "input-a.uasset"),
    path.resolve(fixtureRoot, "input-b.uasset"),
  ];
  return runInspector(process.execPath, inputs, outputDirectory, {
    inspectorPath: makeMockInspector(mode),
  });
}

function runStaleOutputMock() {
  const outputDirectory = fs.mkdtempSync(path.join(fixtureRoot, "parse-stale-"));
  const inputs = [
    path.resolve(fixtureRoot, "input-a.uasset"),
    path.resolve(fixtureRoot, "input-b.uasset"),
  ];
  fs.writeFileSync(
    path.join(outputDirectory, "batch-result.json"),
    JSON.stringify({ results: inputs.map((input) => ({ input, success: true })) }),
  );
  return runInspector(process.execPath, inputs, outputDirectory, {
    inspectorPath: makeMockInspector("ok"),
  });
}

test("inspector process and result boundary requires a fresh, complete batch", () => {
  const result = runMock("ok");
  assert.equal(result.results.length, 2);
  assert.deepEqual(result.results.map((row) => row.input), [
    path.resolve(fixtureRoot, "input-a.uasset"),
    path.resolve(fixtureRoot, "input-b.uasset"),
  ]);
});

test("failed inspector cannot fall back to an output written before its failure", () => {
  assert.throws(() => runMock("fail"), /exit code 7/);
});

test("successful inspector without a new result is rejected", () => {
  assert.throws(() => runMock("missing"), /no batch result/);
});

test("inspector rejects duplicate requested inputs", () => {
  assert.throws(() => runMock("duplicate"), /duplicate input/);
});

test("inspector rejects stale pre-existing results", () => {
  assert.throws(() => runStaleOutputMock(), /not fresh/);
});

test("inspector rejects mismatched requested inputs", () => {
  assert.throws(() => runMock("mismatch"), /unexpected input/);
});
