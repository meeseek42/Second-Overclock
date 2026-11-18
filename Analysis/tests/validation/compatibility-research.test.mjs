import test from "node:test";
import assert from "node:assert/strict";
import fs from "node:fs";

test("current research and replay retain external state and diagnostic storage boundary", () => {
  const source = fs.readFileSync(new URL("../../../Mod/Source/DOREditor/Private/DORGenerateAssetsCommandlet.cpp", import.meta.url), "utf8");
  for (const field of ["ResearchNetMode", "ResearchManualClassifications", "ReplayLastPlayerKey", "ReplayLastGeneration", "ReplayPlayerKey", "RemoteLastPlayerKey", "RemoteRequestGeneration", "RemoteLastTestIndex", "RemoteLastGeneration", "RemoteLastSucceeded"])
    assert.ok(source.includes(field));
  for (const value of ["all-installed", "!dor test hostpair ", "!dor test classify ", "!dor test reward all", "DIAGNOSTIC_SESSION_START", "DIAGNOSTIC_SESSION_END"])
    assert.ok(source.includes(value));
  const storage = source.slice(source.indexOf("bool BuildDiagnosticStorageFunctions"), source.indexOf("bool BuildDebugNotifyFunction"));
  assert.doesNotMatch(storage, /UFSDSaveGame|DOR_CommitNativeSave|SaveSlotToDisk|SaveToDisk|RecoveryJournal/);
  assert.doesNotMatch(source, /SaveGames\/Mods\/DualOverclockReward\/diagnostic-current\.txt/);
  const identity = source.slice(source.indexOf("bool BuildDORStableIdentityResolver"), source.indexOf("UBlueprint* CreatePresetRecoveryBlueprint"));
  assert.doesNotMatch(identity, /GetPlayerName/);
  const dispatch = source.slice(source.indexOf("bool BuildRemoteResearchDispatchFunctions"), source.indexOf("bool BuildConcurrentRewardFunctions"));
  const admission = dispatch.slice(dispatch.indexOf("const FDORStablePlayerKeyPins DispatchIdentity"), dispatch.indexOf("UK2Node_VariableGet* SessionPlayers"));
  assert.doesNotMatch(admission, /HandshakeCompatibleNames|GetPlayerName/);
});

test("diagnostic collector prefers fresh valid plaintext and preserves every source", () => {
  const collector = fs.readFileSync(
    new URL("../../../Scripts/Collect-DORDiagnostics.ps1", import.meta.url),
    "utf8",
  );
  assert.match(collector, /FSD\\Mods\\DualOverclockReward\\diagnostic-current\.txt/);
  assert.match(collector, /FSD\\Mods\\DualOverclockReward\\diagnostic-previous-/);
  assert.match(collector, /FSD\\Mods\\SaveGames\\Mods\\DualOverclockReward\\diagnostic-current\.txt/);
  assert.match(collector, /FSD\\Saved\\SaveGames\\Mods\\DualOverclockReward\\diagnostic-current\.txt/);
  assert.match(collector, /FSD\\Saved\\SaveGames\\Mods\\DOR_Diagnostics_v1\.sav/);
  assert.match(collector, /schema-header-ok/);
  assert.match(collector, /Sort-Object.*lastWriteUtc/s);
  assert.match(collector, /selectedSourceLastWriteUtc/);
  assert.match(collector, /\$\(\$result\.id\)\.txt/);
  assert.match(collector, /selected-diagnostic-current\.txt/);
  assert.match(collector, /copiedSourceCount/);
  assert.doesNotMatch(collector, /Remove-Item|Move-Item/);
});
