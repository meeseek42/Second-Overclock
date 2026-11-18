import {effectiveNumericResult} from './effective-numeric-output.mjs';
import {loadPublicFamily} from '../authoring/prepared-inputs.mjs';
import { resolveInput } from "../shared/paths.mjs";
import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { projectDir } from "../shared/paths.mjs";
import { placeNumericOccurrences, validateNativeSelection } from "./native-numeric-policy.mjs";

export const gf02ContractPath = path.join(projectDir, "Analysis/data/manual/gf02-recipes.json");
export const gf02ResolvedInputsPath = path.join(projectDir, "Analysis/data/manual/gf02-resolved-native-inputs.json");
const hash = (value) => crypto.createHash("sha256").update(value).digest("hex");
const equal = (a, b) => JSON.stringify(a) === JSON.stringify(b);
function requireFact(condition, message) { if (!condition) throw new Error(`GF02: ${message}`); }

// The executor invokes this after runtime generation. All 155 fixed GF01 rows
// must retain their baseline row hashes before the full-file pin may advance.
export function pinGF01Runtime() {
  const filename = path.join(projectDir, "Analysis/research/implementation-wave4-gf01/batch-contract/pair-contract.json");
  const runtimePath = path.join(projectDir, "Mod/Generated/runtime-pairs.json");
  const contract = JSON.parse(fs.readFileSync(resolveInput(filename), "utf8"));
  const runtimeBytes = fs.readFileSync(resolveInput(runtimePath));
  const runtime = JSON.parse(runtimeBytes);
  requireFact(contract.pairs.length === 155, "fixed GF01 count changed");
  for (const expected of contract.pairs) {
    const actual = runtime.pairs.find((row) => row.pairId === expected.pairId);
    requireFact(actual?.rowHash === expected.rowHash && actual.testIndex === expected.testIndex &&
      equal(actual.finalLeafObjectPaths, expected.orderedLeafObjectPaths), `GF01 projection changed: ${expected.pairId}`);
  }
  contract.authority.sourceHashes.runtime = hash(runtimeBytes);
  fs.writeFileSync(filename, `${JSON.stringify(contract, null, 2)}\n`);
  return contract.authority.sourceHashes.runtime;
}

// Contract/runtime source hashes are provenance. The runtime output must never
// be pinned to the Alpha122 input file that this overlay replaces.
export function loadGF02Inputs(contractPath = gf02ContractPath, evidencePath = gf02ResolvedInputsPath, preparedReceipt = null) {
  if(preparedReceipt){const result=loadPublicFamily(preparedReceipt,"gf02",projectDir);validateGF02ResolvedInputs(result.contract,result.evidence);return result;}
  const contractBytes = fs.readFileSync(resolveInput(contractPath));
  const contract = JSON.parse(contractBytes);
  for (const name of ["classification", "implementations", "catalog", "numericPolicy"]) {
    const actual = hash(fs.readFileSync(resolveInput(path.resolve(projectDir, contract.authority.paths[name]))));
    requireFact(actual === contract.authority.sourceHashes[name].toLowerCase(), `pinned input changed: ${name}`);
  }
  const evidenceBytes = fs.readFileSync(resolveInput(evidencePath));
  const evidence = JSON.parse(evidenceBytes);
  requireFact(evidence.contractSha256?.toLowerCase() === hash(contractBytes), "resolved-input contract hash mismatch");
  for (const input of evidence.inputs ?? []) {
    for (const ref of [input.exportEvidence, input.packageEvidence, ...(input.nativeEvidence ?? [])]) {
      requireFact(ref?.path && /^[a-f0-9]{64}$/i.test(ref.sha256 ?? ""), `missing evidence reference: ${input.sourceObjectPath}`);
      requireFact(hash(fs.readFileSync(resolveInput(path.resolve(projectDir, ref.path)))) === ref.sha256.toLowerCase(), `evidence hash mismatch: ${ref.path}`);
    }
  }
  validateGF02ResolvedInputs(contract, evidence);
  return { contract, evidence, contractSha256: hash(contractBytes), evidenceSha256: hash(evidenceBytes) };
}

export function validateGF02ResolvedInputs(contract, evidence) {
  requireFact(evidence.schema === "dor.gf02.resolved-native-inputs.v1", "resolved-input schema mismatch");
  const inputs = new Map();
  for (const input of evidence.inputs ?? []) {
    requireFact(!inputs.has(input.sourceObjectPath), `duplicate resolved input: ${input.sourceObjectPath}`);
    inputs.set(input.sourceObjectPath, input);
  }
  for (const row of contract.recipes) for (const op of row.operations) {
    for (const occurrence of op.sourceOccurrences) {
      const input = inputs.get(occurrence.sourceObjectPath);
      const context = `${row.testIndex}/${row.pairId}:${occurrence.sourceObjectPath}`;
      requireFact(input?.classPath === op.sourceClassPath, `current class mismatch: ${context}`);
      for (const [name, fact] of Object.entries(occurrence)) {
        if (fact && typeof fact === "object" && fact.field) {
          requireFact(equal(input.fields?.[fact.field], fact.value), `current ${fact.field} mismatch: ${context}`);
        }
      }
    }
    for (const gate of op.executionGate.requirements ?? []) {
      const input = inputs.get(gate.sourceObjectPath);
      const basis=evidence.provenance?.schema==='dor.public-prepared.v1'?evidence.expectationShippingExeSha256:evidence.shippingExeSha256;
      requireFact(basis?.toUpperCase() === gate.shippingExeSha256, `Shipping hash mismatch: ${row.testIndex}`);
      requireFact(input.classPath === gate.expectedClass && input.fields?.UpgradeType === gate.expectedTarget &&
        input.fields?.IsPercentage === gate.expectedIsPercentage && input.fields?.[gate.field] === gate.expectedEffectiveValue,
      `conditional field mismatch: ${row.testIndex}/${row.pairId}`);
      requireFact(input.archetype === "native-class-CDO" && gate.allowedCurrentAmount.includes(input.amountState) &&
        input.nativeEvidence?.length > 0, `conditional current export/archetype/native evidence missing: ${row.testIndex}/${row.pairId}`);
    }
  }
}

export function overlayGF02Recipes(pairs, contract, evidence = null) {
  requireFact(contract.schema === "dor.gf02.batch-recipes.v1" && contract.recipes.length === 105, "contract schema/count mismatch");
  if (evidence) validateGF02ResolvedInputs(contract, evidence);
  const byId = new Map(contract.recipes.map((row) => [row.pairId, row]));
  requireFact(byId.size === 105 && new Set(contract.recipes.map((row) => row.testIndex)).size === 105, "duplicate identity");
  let matched = 0;
  const result = pairs.map((row) => {
    const recipe = byId.get(row.pairId);
    if (!recipe) return row;
    ++matched;
    for (const name of ["testIndex", "classKey", "slot", "weaponFolder", "weaponIdObjectPath", "pairUpgradeObjectPath", "overclockAId", "overclockBId"])
      requireFact(row[name] === recipe[name], `stable join mismatch: ${row.pairId}.${name}`);
    requireFact(equal(row.orderedLeafObjectPaths, recipe.orderedLeafObjectPaths), `original occurrence order mismatch: ${row.pairId}`);
    const consumed = new Map();
    const merged = recipe.operations.flatMap((op) => {
      validateNativeSelection(op);
      if (op.operation === "ordered-native-integer-contributions") {
        for (const source of op.sourceOccurrences)
          requireFact(row.orderedLeafObjectPaths[source.index] === source.sourceObjectPath, `retained occurrence mismatch: ${row.pairId}`);
        requireFact(equal(op.outputLeafObjectPaths, op.sourceOccurrences.map(s => s.sourceObjectPath)), `integer native order mismatch: ${row.pairId}`);
        return [];
      }
      requireFact(op.identity.classPath === op.sourceClassPath && ["Amount", "Damage"].includes(op.valueField), `numeric identity mismatch: ${row.pairId}`);
      requireFact(["new-native-standard", "duplicate-native-template"].includes(op.assetCreation), `creation mode: ${row.pairId}`);
      for (const source of op.sourceOccurrences) {
        requireFact(row.orderedLeafObjectPaths[source.index] === source.sourceObjectPath && !consumed.has(source.index), `consumed occurrence mismatch: ${row.pairId}/${source.index}`);
        consumed.set(source.index, { first: op.sourceOccurrences[0].index, path: op.mergedObjectPath });
      }
      requireFact(Number.isFinite(op.serializedResult) && effectiveNumericResult(op) === op.serializedResult, `final float32 mismatch: ${row.pairId}`);
      requireFact(!op.isPercentage || op.serializedResult >= 0, `illegal negative factor: ${row.pairId}`);
      requireFact(op.selection.candidates.some((candidate) => candidate.mode === op.selection.mode &&
        candidate.legal && candidate.value === op.selection.value), `selected candidate mismatch: ${row.pairId}`);
      return [{ ...op, sourceObjectPath: op.templateObjectPath,
        sourceObjectPaths: op.sourceOccurrences.map((source) => source.sourceObjectPath),
        sourceAmounts: op.sourceOccurrences.map((source) => source.amount.value),
        sourceAmount: op.sourceOccurrences[0].amount.value,
        upgradeType: op.percentageField ? op.identity.target : "",
        mergedAmount: op.serializedResult, contributionCount: op.sourceOccurrences.length,
        mergeMode: op.selection.mode, utilityDirection: op.selection.utility,
        additiveCandidate: op.selection.candidates.find((candidate) => candidate.mode !== "multiplicative")?.value ?? null,
        multiplicativeCandidate: op.selection.candidates.find((candidate) => candidate.mode === "multiplicative")?.value ?? null,
        selectionRule: op.selection.rule, executionGateResolved: Boolean(evidence) }];
    });
    const final = placeNumericOccurrences(row.orderedLeafObjectPaths, recipe.operations);
    requireFact(equal(final, recipe.finalLeafObjectPaths), `final occurrence order mismatch: ${row.pairId}`);
    const { rowHash, ...base } = row;
    return { ...base, finalLeafObjectPaths: final, mergedSharedLeaves: merged,
      adjustedLeaves: [], numericStackDecisions: merged.map((leaf) => ({ target: leaf.identity.target,
        sourceObjectPaths: leaf.sourceObjectPaths, mergedAmount: leaf.mergedAmount,
        mode: leaf.mergeMode, utility: leaf.utilityDirection, runtimeMerge: true })), gf02: true };
  });
  requireFact(matched === 105, `missing runtime joins: ${matched}/105`);
  return result;
}
