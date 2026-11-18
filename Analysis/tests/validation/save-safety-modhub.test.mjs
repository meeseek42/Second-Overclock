import test from "node:test";
import assert from "node:assert/strict";
import fs from "node:fs";

const read = (name) => fs.readFileSync(new URL(`../../../Mod/Source/DOREditor/Private/${name}`, import.meta.url), "utf8");
const safety = read("DORSaveSafety.inl");
const hub = read("DORModHub.inl");
const main = read("DORGenerateAssetsCommandlet.cpp");

test("current safety keeps profile, journal and transaction identity fields", () => {
  for (const field of ["SourceSlot", "SourceAnonymousId", "SourceIndex", "Schema", "TransactionId"])
    assert.ok(safety.includes(`TEXT("${field}")`));
});

test("current native backup and legacy recovery namespaces stay distinct", () => {
  for (const name of ["DOR_FirstInstallBackup_", "Mods/DOR_FirstInstallBackup_", "DOR_PresetRecovery_v1", "Mods/DOR_PresetRecovery_v1"])
    assert.ok(safety.includes(name));
});

test("current manual settings retain flat compatibility fields", () => {
  for (const field of ["Key%d", "Pair%d", "IdKey%d", "IdPair%d", "IdPrimaryPair%d", "IdSecondaryPair%d"])
    assert.ok(hub.includes(`TEXT("${field}")`));
});

test("current stable identity and selection do not parse display labels", () => {
  const key = hub.slice(hub.indexOf("FDORStablePlayerKeyPins DORPlayerIdentity"), hub.indexOf("UEdGraphPin* DORPlayerLabel"));
  assert.doesNotMatch(key, /GetPlayerName/);
  const choice = hub.slice(hub.indexOf('FDORSafeGraph F(Page,TEXT("DOR_SaveChoice"))'), hub.indexOf('FDORSafeGraph F(Page,TEXT("DOR_CleanupClicked"))'));
  assert.doesNotMatch(choice, /GetSelectedOption|Conv_StringToInt/);
});

test("current graph audit has explicit provenance and the generator rejects stale output", () => {
  const generate = fs.readFileSync(
    new URL("../../../Scripts/Generate-DORAssets.ps1", import.meta.url),
    "utf8",
  );
  assert.match(hub, /DORGraphAudit\.json/);
  assert.match(hub, /SetNumberField\(TEXT\("schema"\),2\)/);
  assert.match(hub, /SetStringField\(TEXT\("manifestHash"\),ManifestHash\)/);
  assert.match(hub, /generatedAtUtc/);
  assert.match(hub, /DORGenerateAssetsCommandlet\.cpp\+DORModHub\.inl/);
  assert.match(hub, /DualSlotSavedFields==8/);
  assert.match(hub, /DualSlotReaderGraphs==2/);
  assert.match(hub, /DualSlotWriterInputs==4/);
  assert.match(hub, /SlotBoundConsumeInputs==1/);
  assert.match(hub, /pageDualSlotSetCalls/);
  assert.doesNotMatch(hub, /DORAlpha73GraphAudit\.json/);
  assert.match(generate, /Remove-Item -LiteralPath \$auditPath/);
  assert.match(generate, /LastWriteTimeUtc -lt \$generationStartedUtc/);
  assert.match(generate, /audit\.manifestHash -ne \$runtimeManifest\.manifestHash/);
  assert.match(generate, /audit\.generatorSource/);
});
