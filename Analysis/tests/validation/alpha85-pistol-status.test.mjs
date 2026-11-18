import test from "node:test";
import assert from "node:assert/strict";
import fs from "node:fs";
import {
  ACCEPTED_BUILD_TAG,
  EXPECTED_PACKAGE_FILENAME,
  readCurrentArtifactIdentity,
} from "./current-artifact-identity.mjs";

const sourceUrl = new URL(
  "../../../Mod/Source/DOREditor/Private/DORGenerateAssetsCommandlet.cpp",
  import.meta.url,
);
const source = fs.readFileSync(sourceUrl, "utf8");
const gf01Contract = JSON.parse(
  fs.readFileSync(
    new URL(
      "../../research/implementation-wave4-gf01/batch-contract/pair-contract.json",
      import.meta.url,
    ),
    "utf8",
  ),
);
test("current 359 diagnostic component identity is pinned", () => {
  const authoritativeClass = "BP_ExplosiveReloadComponent.BP_ExplosiveReloadComponent_C";
  assert.ok(source.includes(authoritativeClass));
  assert.doesNotMatch(source, /Pistol\/Overclocks\/BP_ExplosiveReloadComponent\.BP_ExplosiveReloadComponent_C/);
});

test("pair 359 leaves and supported set stay pinned with current package identity", () => {
  const manifest = JSON.parse(
    fs.readFileSync(
      new URL("../../../Mod/Generated/runtime-pairs.json", import.meta.url),
      "utf8",
    ),
  );
  const pair359 = manifest.pairs.find((row) => row.testIndex === 359);
  assert.ok(pair359);
  assert.equal(pair359.classKey, "driller");
  assert.equal(pair359.slot, "secondary");
  assert.equal(
    pair359.weaponObjectPath,
    "/Game/WeaponsNTools/Pistol/WPN_Pistol_A.WPN_Pistol_A",
  );
  assert.deepEqual(pair359.finalLeafObjectPaths.map((path) => path.split(".").at(-1)), [
    "OC_Bonus_ExplosiveReload",
    "Penalty_Ammo-50p_Pistol",
    "Penalty_ClipSize-50p_Pistol",
    "OC_Bonus_FullAuto_Pistol",
    "OC_Bonus_RoF_Pistol",
    "OC_Penalty_Recoil_Pistol",
    "OC_Penalty_BaseAccuracy_Pistol",
  ]);
  const explosiveLeaf = manifest.overclocks
    .flatMap((row) => row.leafObjectPaths ?? [])
    .filter((path) => path.endsWith("OC_Bonus_ExplosiveReload.OC_Bonus_ExplosiveReload"));
  assert.equal(new Set(explosiveLeaf).size, 1);
  const legacySet = source.match(
    /TSet<int32>& DORSupportedPairTestIndices\(\)[\s\S]*?Indices\s*=\s*\{([^}]*)\};/,
  );
  assert.ok(legacySet, "legacy supported-index set is present");
  const legacyIndices = legacySet[1]
    .split(",")
    .map((value) => Number(value.trim()))
    .filter((value) => Number.isInteger(value));
  assert.deepEqual(legacyIndices, [277, 359, 385, 388, 435, 438, 444]);
  assert.equal(new Set(legacyIndices).size, legacyIndices.length);

  const candidateRows = gf01Contract.pairs.filter(
    (row) => row.admission === "candidate-only",
  );
  assert.equal(candidateRows.length, 154);
  assert.equal(legacyIndices.length + candidateRows.length, 161);
  const admissionStart = source.indexOf("bool DORAdmitGF01Candidates()");
  const admissionEnd = source.indexOf("bool ReadGF01Contract", admissionStart);
  assert.ok(admissionStart >= 0 && admissionEnd > admissionStart);
  const admission = source.slice(admissionStart, admissionEnd);
  const admissionGuard = admission.match(
    /Supported\.Num\(\)\s*!=\s*(\d+)\s*\|\|\s*GF01CandidateSpecs\.Num\(\)\s*!=\s*(\d+)/,
  );
  assert.deepEqual(
    admissionGuard?.slice(1).map(Number),
    [legacyIndices.length, candidateRows.length],
  );
  assert.match(
    admission,
    /for\s*\(const FDORGF01PairSpec& Spec : GF01CandidateSpecs\)[\s\S]*?Supported\.Add\(Spec\.TestIndex\)/,
  );
  const readContractCall = source.indexOf("if (!ReadGF01Contract");
  const admissionCall = source.indexOf("if (!DORAdmitGF01Candidates", readContractCall);
  assert.ok(readContractCall >= 0);
  assert.ok(readContractCall < admissionCall);
  const generationScript = fs.readFileSync(
    new URL("../../../Scripts/Generate-DORAssets.ps1", import.meta.url),
    "utf8",
  );
  const packageScript = fs.readFileSync(
    new URL("../../../Scripts/Package-DOR.ps1", import.meta.url),
    "utf8",
  );
  const identity = readCurrentArtifactIdentity({
    generatorSource: source,
    generationScript,
    packageScript,
  });
  assert.equal(identity.buildTag, ACCEPTED_BUILD_TAG);
  assert.equal(identity.expectedBuild, identity.buildTag);
  assert.equal(identity.packageFilename, EXPECTED_PACKAGE_FILENAME);

  const commentOnlyPackage = packageScript.replace(
    "Historical diagnostics artifact is retained and must never be overwritten: DualOverclockReward-alpha86-apply001-diagnostics.pak",
    "Historical diagnostics artifact is retained and must never be overwritten: DualOverclockReward-alpha103-safe-owner-rearm-final.pak",
  );
  assert.doesNotThrow(() => readCurrentArtifactIdentity({
    generatorSource: source,
    generationScript,
    packageScript: commentOnlyPackage,
  }));
  assert.throws(() => readCurrentArtifactIdentity({
    generatorSource: source.replace(identity.buildTag, "0.67.0-alpha104-safe-owner-rearm"),
    generationScript,
    packageScript,
  }));
  assert.throws(() => readCurrentArtifactIdentity({
    generatorSource: source,
    generationScript: generationScript.replace(
      "$expectedBuild = '" + identity.expectedBuild + "'",
      "$expectedBuild = '0.67.0-alpha104-safe-owner-rearm'",
    ),
    packageScript,
  }));
  assert.throws(() => readCurrentArtifactIdentity({
    generatorSource: source,
    generationScript,
    packageScript: packageScript.replace(identity.packageFilename, "DualOverclockReward-alpha102-checked-legacy-retirement-final.pak"),
  }));
});
