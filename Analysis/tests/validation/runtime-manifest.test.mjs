import test from "node:test";
import assert from "node:assert/strict";
import fs from "node:fs";
import crypto from "node:crypto";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { buildRuntimeManifest } from "../../src/runtime/build-runtime-manifest.mjs";
import { readJson } from "../../src/shared/io.mjs";

const fixtureRoot = fileURLToPath(new URL(
  "../../research/phase-review-20261008/unit5-current-suite-maintenance/temp/",
  import.meta.url,
));

function createRuntimeFixture(label) {
  fs.mkdirSync(fixtureRoot, { recursive: true });
  const outputDir = fs.mkdtempSync(path.join(fixtureRoot, `${label}-`));
  const outputPaths = {
    runtimeManifest: path.join(outputDir, "runtime-pairs.json"),
    runtimePairsCsv: path.join(outputDir, "runtime-pairs.csv"),
    runtimeTestIndexCsv: path.join(outputDir, "pair-test-index.csv"),
  };
  return { manifest: buildRuntimeManifest(outputPaths), outputPaths };
}

const runtimeFixture = createRuntimeFixture("runtime-manifest");
test("runtime manifest contains the current catalog", () => {
  const manifest = runtimeFixture.manifest;
  assert.equal(manifest.overclocks.length, 160);
  assert.equal(manifest.pairs.length, 456);
  assert.equal(manifest.metadata.excludedPairCount, 5);
  assert.deepEqual(
    manifest.pairs.reduce((counts, row) => {
      counts[row.route] = (counts[row.route] ?? 0) + 1;
      return counts;
    }, {}),
    { "直接叠加": 438, "顺序保护": 12, "轻量桥接": 2, "定制融合": 4 },
  );
  assert.ok(manifest.pairs.every((row) => row.implementationComplexity < 4 || row.remainingPrepared));
});

test("runtime pair rows have deterministic leaves, hashes, and indices", () => {
  const first = runtimeFixture.manifest;
  const secondFixture = createRuntimeFixture("runtime-determinism");
  const second = secondFixture.manifest;
  assert.equal(first.manifestHash, second.manifestHash);
  assert.deepEqual(first.pairs, second.pairs);
  for (const filename of Object.keys(runtimeFixture.outputPaths)) {
    assert.deepEqual(
      fs.readFileSync(runtimeFixture.outputPaths[filename]),
      fs.readFileSync(secondFixture.outputPaths[filename]),
      `${filename} is deterministic`,
    );
  }
  assert.deepEqual(first.pairs.map((row) => row.testIndex), [...Array(456)].map((_, i) => i + 1));
  for (const row of first.pairs) {
    if (!row.gf02 && !row.gf03 && !row.remainingPrepared) assert.equal(new Set(row.finalLeafObjectPaths).size, row.finalLeafObjectPaths.length);
    assert.match(row.rowHash, /^[0-9a-f]{64}$/);
  }
  const mergedPairs = first.pairs.filter((row) => row.mergedSharedLeaves.length);
  assert.equal(mergedPairs.length, 152);
  assert.equal(mergedPairs.flatMap((row) => row.mergedSharedLeaves).length, 195);
});

test("GF-04 representative 359 preserves the formal seven-leaf component union", () => {
  const manifest = runtimeFixture.manifest;
  const pair = manifest.pairs.find((row) => row.testIndex === 359);
  assert.equal(pair.pairId, "8d77dbb8f0914479");
  assert.equal(pair.classKey, "driller");
  assert.equal(pair.slot, "secondary");
  assert.equal(pair.weaponFolder, "Pistol");
  assert.equal(pair.fusionRootPath,
    "/Game/DualOverclockReward/Pairs/Pistol/DORPair_359_8d77dbb8f0914479.DORPair_359_8d77dbb8f0914479");
  assert.deepEqual(pair.finalLeafObjectPaths.map((value) => value.split("/").at(-1).split(".")[0]), [
    "OC_Bonus_ExplosiveReload",
    "Penalty_Ammo-50p_Pistol",
    "Penalty_ClipSize-50p_Pistol",
    "OC_Bonus_FullAuto_Pistol",
    "OC_Bonus_RoF_Pistol",
    "OC_Penalty_Recoil_Pistol",
    "OC_Penalty_BaseAccuracy_Pistol",
  ]);
  assert.deepEqual(pair.duplicateLeafObjectPaths, []);
  assert.deepEqual(pair.mergedSharedLeaves, []);
  assert.deepEqual(pair.adjustedLeaves, []);
  assert.deepEqual(pair.numericStackDecisions, []);

  const formalPath = new URL(
    "../../research/fusion-classification/fusion-classification-master.json",
    import.meta.url,
  );
  const formalBytes = fs.readFileSync(formalPath);
  assert.equal(
    crypto.createHash("sha256").update(formalBytes).digest("hex"),
    "3100acad57f84d33a5f2891b3c6832055827feddd448158376c50bb5b1e88057",
  );
  const formal = JSON.parse(formalBytes);
  const row = formal.rows.find((value) => value.pairId === pair.pairId);
  assert.equal(row.displayNumber, 368);
  assert.equal(row.finalClassification.globalFamilyId,
    "GF-04-ORIGINAL-EVENT-COMPONENT-UNION");
  assert.match(row.finalClassification.approvedBaseline.boundary,
    /单个WeaponHitCounterComponent/);
});

test("pair 359 stock evidence keeps one native hit-counter owner", () => {
  const sourceEvidence = JSON.parse(fs.readFileSync(
    new URL(
      "../../research/fusion-classification/batches/017-subata/source-evidence.json",
      import.meta.url,
    ),
    "utf8",
  ));
  const contributingOverclockIds = new Set(["ae870f9e484cd5a5", "27b075a4970c1c81"]);
  const hitCounterOwners = sourceEvidence.overclocks
    .filter((overclock) => contributingOverclockIds.has(overclock.overclockId))
    .flatMap((overclock) => overclock.leaves)
    .filter((leaf) => leaf.classNames.includes("WeaponHitCounterUpgrade"))
    .map((leaf) => leaf.packagePath);
  assert.deepEqual(hitCounterOwners, [
    "/Game/WeaponsNTools/Pistol/Overclocks/OC_BonusesAndPenalties/OC_Bonus_ExplosiveReload",
  ]);

});

test("pair 385 replaces overlapping penalties but retains both special event leaves", () => {
  const manifest = runtimeFixture.manifest;
  const pair385 = manifest.pairs.find((row) => row.testIndex === 385);
  assert.equal(pair385.pairId, "cbdb1b90f7da184b");
  assert.equal(pair385.mergedSharedLeaves.length, 2);

  const heat = pair385.mergedSharedLeaves.find((leaf) =>
    leaf.upgradeType === "EInventoryItemUpgradeType::ManualHeatPerUse");
  assert.deepEqual(heat.sourceAmounts, [1.2000000476837158, 1.5]);
  assert.ok(Math.abs(heat.additiveCandidate - 1.7) < 0.000001);
  assert.ok(Math.abs(heat.multiplicativeCandidate - 1.8) < 0.000001);
  assert.equal(heat.mergeMode, "multiplicative");
  assert.equal(heat.mergedAmount, 1.8000000715255737);
  assert.equal(heat.selectionRule, "preserve-known-native-operator");

  const overheat = pair385.mergedSharedLeaves.find((leaf) =>
    leaf.upgradeType === "EInventoryItemUpgradeType::UnJamDuration");
  assert.deepEqual(overheat.sourceAmounts, [1.2999999523162842, 1]);
  assert.equal(overheat.mergeMode, "strongest-single");
  assert.equal(overheat.mergedAmount, 1.3);

  for (const merged of pair385.mergedSharedLeaves) {
    assert.ok(pair385.finalLeafObjectPaths.includes(merged.mergedObjectPath));
    for (const sourcePath of merged.sourceObjectPaths) {
      assert.ok(!pair385.finalLeafObjectPaths.includes(sourcePath));
    }
  }
  assert.ok(pair385.finalLeafObjectPaths.some((path) => path.includes("Bonus_HotPlasma_PC")));
  assert.ok(pair385.finalLeafObjectPaths.some((path) => path.includes("Bonus_ShieldPlasma_PC")));
  assert.ok(pair385.finalLeafObjectPaths.some((path) =>
    path.includes("Penalty_RemoveShieldOnOverheat_PlasmaCarbine")));
});

test("pair 435 preserves the seven distinct stock SMG behavior leaves", () => {
  const manifest = runtimeFixture.manifest;
  const pair435 = manifest.pairs.find((row) => row.testIndex === 435);
  assert.ok(pair435);
  assert.equal(pair435.pairId, "79c731e7dd86051c");
  assert.equal(pair435.classKey, "engineer");
  assert.equal(pair435.slot, "primary");
  assert.equal(pair435.weaponFolder, "SMG");
  assert.equal(pair435.finalLeafObjectPaths.length, 7);
  assert.equal(new Set(pair435.finalLeafObjectPaths).size, 7);
  assert.deepEqual(pair435.duplicateLeafObjectPaths, []);
  assert.deepEqual(pair435.mergedSharedLeaves, []);
  assert.deepEqual(pair435.adjustedLeaves, []);
  assert.deepEqual(pair435.numericStackDecisions, []);
  assert.deepEqual(pair435.sharedWriteTargets, []);
  assert.equal(pair435.rowHash, "aa75b25d909e5a1192feda3965cc139d2da920650a08bbb91f0df1ffb4aa7354");
  for (const leaf of [
    "Bonus_ElectrifyPlatform_SMG",
    "Bonus_PlasmaBeam_SMG",
    "Bonus_EMPDischarge_SMG",
    "Penalty_Ammo-120_SMG",
    "Penalty_RoF2_SMG",
    "Penalty_ClipSize_SMG",
    "Penlaty_DMG-3_SMG",
  ]) assert.ok(pair435.finalLeafObjectPaths.some((value) => value.includes(leaf)));
});

test("pair 444 joins by stable ID and preserves the exact five-leaf SMG union", () => {
  const manifest = runtimeFixture.manifest;
  const pair444 = manifest.pairs.find((row) => row.pairId === "db0dc6f2b8b4f8ed");
  assert.ok(pair444);
  assert.equal(pair444.testIndex, 444);
  assert.equal(pair444.classKey, "engineer");
  assert.equal(pair444.slot, "primary");
  assert.equal(pair444.weaponFolder, "SMG");
  assert.equal(
    pair444.pairUpgradeObjectPath,
    "/Game/DualOverclockReward/Pairs/SMG/" +
      "DORPair_444_db0dc6f2b8b4f8ed.DORPair_444_db0dc6f2b8b4f8ed",
  );
  assert.deepEqual(pair444.orderedLeafObjectPaths, [
    "/Game/WeaponsNTools/SMG/Overclocks/OC_BonusesAndPenalties/Bonus_ReloadTime02_SMG.Bonus_ReloadTime02_SMG",
    "/Game/WeaponsNTools/SMG/Overclocks/OC_BonusesAndPenalties/Bonus_RoF2_SMG.Bonus_RoF2_SMG",
    "/Game/WeaponsNTools/SMG/Overclocks/OC_BonusesAndPenalties/Bonus_EMPDischarge_SMG.Bonus_EMPDischarge_SMG",
    "/Game/WeaponsNTools/SMG/Overclocks/OC_BonusesAndPenalties/Penalty_ClipSize_SMG.Penalty_ClipSize_SMG",
    "/Game/WeaponsNTools/SMG/Overclocks/OC_BonusesAndPenalties/Penlaty_DMG-3_SMG.Penlaty_DMG-3_SMG",
  ]);
  assert.deepEqual(pair444.finalLeafObjectPaths, pair444.orderedLeafObjectPaths);
  assert.deepEqual(pair444.duplicateLeafObjectPaths, []);
  assert.deepEqual(pair444.mergedSharedLeaves, []);
  assert.deepEqual(pair444.adjustedLeaves, []);
  assert.deepEqual(pair444.numericStackDecisions, []);
  assert.deepEqual(pair444.sharedWriteTargets, []);
  assert.equal(
    pair444.rowHash,
    "45e4373ece83b8fda331e3da88fd8a6f2a4c789cf21610c526ce39285001f9ec",
  );
  assert.equal(pair444.implementationComplexity, 0);
});

test("GF-02 representative joins by stable ID and recomposes flat RateOfFire once", () => {
  const manifest = runtimeFixture.manifest;
  const pair438 = manifest.pairs.find((row) => row.pairId === "cf63212230811ceb");
  assert.ok(pair438);
  assert.equal(pair438.testIndex, 438);
  assert.equal(pair438.classKey, "engineer");
  assert.equal(pair438.slot, "primary");
  assert.equal(pair438.weaponFolder, "SMG");
  assert.equal(
    pair438.pairUpgradeObjectPath,
    "/Game/DualOverclockReward/Pairs/SMG/" +
      "DORPair_438_cf63212230811ceb.DORPair_438_cf63212230811ceb",
  );
  const rof4 = "/Game/WeaponsNTools/SMG/Overclocks/OC_BonusesAndPenalties/" +
    "Bonus_RoF4_SMG.Bonus_RoF4_SMG";
  const rof2 = "/Game/WeaponsNTools/SMG/Overclocks/OC_BonusesAndPenalties/" +
    "Bonus_RoF2_SMG.Bonus_RoF2_SMG";
  const mergedPath = "/Game/DualOverclockReward/MergedLeaves/SMG/" +
    "DORMerged_cf63212230811ceb_01.DORMerged_cf63212230811ceb_01";
  assert.deepEqual(pair438.orderedLeafObjectPaths, [
    "/Game/WeaponsNTools/SMG/Overclocks/OC_BonusesAndPenalties/Bonus_ElectricDamage_SMG.Bonus_ElectricDamage_SMG",
    rof4,
    "/Game/WeaponsNTools/SMG/Overclocks/OC_BonusesAndPenalties/Penalty_AccuracyBig.Penalty_AccuracyBig",
    "/Game/WeaponsNTools/SMG/Overclocks/OC_BonusesAndPenalties/Bonus_ReloadTime02_SMG.Bonus_ReloadTime02_SMG",
    rof2,
  ]);
  assert.deepEqual(pair438.finalLeafObjectPaths, [
    pair438.orderedLeafObjectPaths[0],
    mergedPath,
    pair438.orderedLeafObjectPaths[2],
    pair438.orderedLeafObjectPaths[3],
  ]);
  assert.deepEqual(pair438.duplicateLeafObjectPaths, []);
  assert.deepEqual(pair438.adjustedLeaves, []);
  assert.equal(pair438.mergedSharedLeaves.length, 1);
  const numeric438 = pair438.mergedSharedLeaves[0];
  assert.deepEqual(Object.fromEntries([
    "sourceObjectPath", "sourceObjectPaths", "sourceClassPath", "mergedObjectPath",
    "sourceAmount", "sourceAmounts", "isPercentage", "upgradeType", "contributionCount",
    "mergeMode", "mergedAmount",
  ].map((name) => [name, numeric438[name]])), {
    sourceObjectPath: rof4,
    sourceObjectPaths: [rof4, rof2],
    sourceClassPath: "/Script/FSD.AmmoDrivenWeaponUpgrade",
    mergedObjectPath: mergedPath,
    sourceAmount: 4,
    sourceAmounts: [4, 2],
    isPercentage: false,
    upgradeType: "EAmmoDrivenWeapnUpgradeType::RateOfFire",
    contributionCount: 2,
    mergeMode: "flat-additive",
    mergedAmount: 6,
  });
  assert.deepEqual(pair438.numericStackDecisions, [{
    target: "EAmmoDrivenWeapnUpgradeType::RateOfFire",
    utility: null,
    mode: "flat-additive",
    runtimeMerge: true,
    mergedAmount: 6,
    sourceObjectPaths: [rof4, rof2],
  }]);
  assert.deepEqual(pair438.sharedWriteTargets, []);
  assert.equal(numeric438.valueField, "Amount");
  assert.equal(numeric438.assetCreation, "new-native-standard");
  assert.equal(numeric438.executionGateResolved, true);
  assert.deepEqual(numeric438.sourceOccurrences.map((source) => source.index), [1, 4]);
});

test("pair 388 exposes the cooked weapon mirror contract without changing other rows", () => {
  const manifest = runtimeFixture.manifest;
  const pair388 = manifest.pairs.find((row) => row.testIndex === 388);
  assert.equal(pair388.pairId, "a2edc4fa1d61b86f");
  assert.equal(
    pair388.weaponMirrorClassPath,
    "/Game/DualOverclockReward/Weapons/PlasmaCarbine/" +
      "WPN_DOR_PlasmaCarbine.WPN_DOR_PlasmaCarbine_C",
  );
  assert.equal(pair388.weaponMirrorSavegameId, "D0388A25-7A11-4B9D-9338-A2EDC4FA1D61");
  assert.equal(
    pair388.nativeItemIdPath,
    "/Game/WeaponsNTools/PlasmaCarbine/ID_PlasmaCarbine.ID_PlasmaCarbine",
  );
  assert.equal(pair388.fusionRootPath, pair388.pairUpgradeObjectPath);
  assert.equal(pair388.pairSpecificWeaponClassPath, "");
  assert.equal(
    manifest.pairs.filter((row) => row.weaponMirrorClassPath).length,
    1,
  );
});

test("identical numeric leaves use the owner polarity policy", () => {
  const manifest = runtimeFixture.manifest;
  const smgPair = manifest.pairs.find((row) => row.pairId === "75408b742807fd22");
  assert.ok(smgPair);
  const smgRate = smgPair.mergedSharedLeaves.find((leaf) => leaf.upgradeType === "EAmmoDrivenWeapnUpgradeType::RateOfFire");
  assert.equal(smgRate.sourceAmount, -2);
  assert.equal(smgRate.mergedAmount, -4);
  assert.equal(smgRate.isPercentage, false);

  const coilgunPair = manifest.pairs.find((row) => row.pairId === "bccaf9883c8f7f6c");
  assert.ok(coilgunPair);
  assert.equal(coilgunPair.mergedSharedLeaves[0].sourceAmount, 0.699999988079071);
  assert.ok(Math.abs(coilgunPair.mergedSharedLeaves[0].mergedAmount - 0.49) < 0.000001);
  assert.equal(coilgunPair.mergedSharedLeaves[0].isPercentage, true);
  assert.equal(coilgunPair.mergedSharedLeaves[0].utilityDirection, "higher-is-better");
  assert.equal(coilgunPair.mergedSharedLeaves[0].mergeMode, "multiplicative");
  assert.deepEqual(coilgunPair.mergedSharedLeaves[0].selection.polarities, ["negative", "negative"]);
  assert.equal(coilgunPair.mergedSharedLeaves[0].selectionRule,
    "player-favorable-negative-or-mixed");
});

test("pair 388 retains its approved percentage-then-flat ordering", () => {
  const manifest = runtimeFixture.manifest;

  const pair388 = manifest.pairs.find((row) => row.pairId === "a2edc4fa1d61b86f");
  assert.ok(pair388);
  assert.equal(pair388.numericStackDecisions.length, 1);
  assert.equal(pair388.numericStackDecisions[0].target,
    "EInventoryItemUpgradeType::ManualHeatPerUse");
  assert.equal(pair388.numericStackDecisions[0].utility, "lower-is-better");
  assert.equal(pair388.numericStackDecisions[0].mode, "percentage-then-flat");
  const sources = pair388.numericStackDecisions[0].sourceObjectPaths;
  assert.ok(pair388.finalLeafObjectPaths.indexOf(sources[1]) < pair388.finalLeafObjectPaths.indexOf(sources[0]));
});

test("GF03 selected mixed penalties retain native contributions and rounding order", () => {
  const manifest = runtimeFixture.manifest;
  for (const [code, target, flat, factor] of [
    [8, "EAmmoDrivenWeapnUpgradeType::RateOfFire", "OC_Penalty_RoF-2_AssaultRifle", "OC_Penalty_RoF-X_AssaultRifle"],
    [257, "EAmmoDrivenWeapnUpgradeType::MaxAmmo", "Penalty_Ammo-2_PGL", "Penalty_Ammo-80p_PGL"],
  ]) {
    const pair = manifest.pairs.find(row => row.testIndex === code);
    assert.equal(pair.numericStackDecisions.find(d => d.target === target).mode, "flat-then-percentage");
    const leaves = pair.finalLeafObjectPaths.map(p => p.split(".").at(-1));
    assert.ok(leaves.indexOf(flat) >= 0 && leaves.indexOf(flat) < leaves.indexOf(factor));
  }
  const pair257 = manifest.pairs.find(row => row.testIndex === 257);
  assert.equal(pair257.adjustedLeaves[0].adjustedAmount, Math.fround(100 / Math.fround(0.3)));
  assert.equal(manifest.pairs.flatMap(row => row.adjustedLeaves).length, 43);
  const pair285 = manifest.pairs.find((row) => row.testIndex === 285);
  assert.deepEqual(pair285.mergedSharedLeaves, []);
  assert.deepEqual(pair285.finalLeafObjectPaths, pair285.orderedLeafObjectPaths);
  assert.deepEqual(pair285.adjustedLeaves, []);
});

test("the two bridge pairs are explicit and no prose inference is required", () => {
  const manifest = readJson(runtimeFixture.outputPaths.runtimeManifest);
  const bridges = manifest.pairs.filter((row) => row.route === "轻量桥接");
  assert.deepEqual(
    bridges.map((row) => row.bridgeHandlerId).sort(),
    ["goo_bomber_disperser_v1", "linecutter_returning_roll_v1"],
  );
  assert.equal(manifest.policy.noMechanicTextInference, true);
});

test("runtime CSV and human test index are emitted", () => {
  assert.ok(fs.statSync(runtimeFixture.outputPaths.runtimePairsCsv).size > 1000);
  assert.ok(fs.statSync(runtimeFixture.outputPaths.runtimeTestIndexCsv).size > 1000);
});

// Historical compatibility-research tiers and commands are not current runtime
// contracts. Their 454-row/seven-pair deployment fixtures remain in research.

test("secondary reward triggers the game's original generic wave controller", () => {
  const source = fs.readFileSync(
    new URL(
      "../../../Mod/Source/DOREditor/Private/DORGenerateAssetsCommandlet.cpp",
      import.meta.url,
    ),
    "utf8",
  );
  const start = source.indexOf(
    'FindFunctionGraph(Master, TEXT("DOR_TriggerAnnouncedSwarm"))',
  );
  const end = source.indexOf(
    "FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Master)",
    start,
  );
  assert.ok(start >= 0 && end > start);
  const dispatchSection = source.slice(start, end);

  assert.match(dispatchSection, /EWC_Generic\.EWC_Generic_C/);
  assert.match(dispatchSection, /TEXT\("TriggerWave"\)/);
  assert.doesNotMatch(dispatchSection, /BP_DOR_Wave_%s/);
  assert.doesNotMatch(dispatchSection, /SpawnScriptedWave/);
  assert.doesNotMatch(dispatchSection, /SpawnEnemiesFromPool/);
  assert.doesNotMatch(dispatchSection, /MissionShout/);
});

test("release packaging includes the cooked asset registry required by the mod loader", () => {
  const pathsScript = fs.readFileSync(
    new URL("../../../Scripts/DOR-Paths.ps1", import.meta.url),
    "utf8",
  );
  const packageScript = fs.readFileSync(
    new URL("../../../Scripts/Package-DOR.ps1", import.meta.url),
    "utf8",
  );
  const generateCmd = fs.readFileSync(
    new URL("../../../Scripts/Generate-DORAssets.cmd", import.meta.url),
    "utf8",
  );

  assert.match(pathsScript, /CookedAssetRegistry/);
  assert.match(pathsScript, /ReparsePoint/);
  assert.match(packageScript, /Assert-DORPath.*CookedAssetRegistry/);
  assert.match(packageScript, /"\.\.\/\.\.\/\.\.\/FSD\/AssetRegistry\.bin"/);
  assert.match(packageScript, /DualOverclockReward/);
  assert.match(generateCmd, /pwsh\.exe -NoProfile -File/);
  assert.match(generateCmd, /Generate-DORAssets\.ps1/);
  assert.match(generateCmd, /exit \/b %exitCode%/);
  assert.doesNotMatch(generateCmd, /powershell\.exe|ExecutionPolicy Bypass|UE4Editor-Cmd|USERPROFILE=R:|APPDATA=R:/);
});

test("current admitted stock identities and native-save prohibitions stay pinned", () => {
  const source = fs.readFileSync(new URL("../../../Mod/Source/DOREditor/Private/DORGenerateAssetsCommandlet.cpp", import.meta.url), "utf8");
  for (const value of ["DORPair_444_db0dc6f2b8b4f8ed", "DORSchematic_444_db0dc6f2b8b4f8ed", "DORPair_438_cf63212230811ceb", "DORSchematic_438_cf63212230811ceb"])
    assert.ok(source.includes(value));
  assert.match(source, /FGuid\(0xDB0DC6F2, 0xB8B4F8ED, 0x53434845, 0x4D413434\)/);
  assert.match(source, /FGuid\(0xCF632122, 0x30811CEB, 0x53434845, 0x4D413338\)/);
  const transaction = source.slice(source.indexOf("bool BuildPair388PresetTransactionFunction"), source.indexOf("bool AddPresetTransactionEndPlayGuard"));
  assert.doesNotMatch(transaction, /SaveToDisk/);
  assert.doesNotMatch(source, /AsyncSaveGameToSlot/);
});

test("current reward objective policy and public controller buttons stay available", () => {
  const source = fs.readFileSync(new URL("../../../Mod/Source/DOREditor/Private/DORGenerateAssetsCommandlet.cpp", import.meta.url), "utf8");
  for (const value of ["Gamepad_FaceButton_Left", "Gamepad_FaceButton_Bottom", "Gamepad_FaceButton_Right", "OBJ_2nd_Find_Fossil", "OBJ_2nd_Find_BooloCap", "OBJ_2nd_Find_ApocaBloom", "OBJ_2nd_Find_Ebonut", "OBJ_2nd_Find_Gunkseed", "OBJ_2nd_Mine_Hollomite", "OBJ_2nd_Mine_Dystrum", "OBJ_2nd_KillFleas", "OBJ_2nd_DestroyBhaBarnacles", "OBJ_2nd_DestroyEggs"])
    assert.ok(source.includes(value));
});
