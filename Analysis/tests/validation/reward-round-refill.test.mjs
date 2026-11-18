import test from "node:test";
import assert from "node:assert/strict";
import fs from "node:fs";

const source = fs.readFileSync(new URL(
  "../../../Mod/Source/DOREditor/Private/DORGenerateAssetsCommandlet.cpp", import.meta.url,
), "utf8");
function section(start, end) {
  const offset = source.indexOf(start);
  const limit = source.indexOf(end, offset + start.length);
  assert.ok(offset >= 0 && limit > offset, `section exists: ${start}`);
  return source.slice(offset, limit);
}

test("current native refill excludes modifier and weapon-rebuild effects", () => {
  const ammo = section("bool BuildAmmoRefillRewardFunctions(", "bool BuildMasterBlueprint(");
  assert.doesNotMatch(ammo, /AddModifier|UpdateFromSaveGameInSlot|Client_Resupply|ForceApplyStatReplication/);
});

test("current choice submission does not finish before the authoritative result", () => {
  const submit = section('UEdGraph* SubmitGraph = FindFunctionGraph(Master, TEXT("DOR_SubmitChoiceForProxy"))', 'UEdGraph* CancelGraph =');
  assert.doesNotMatch(submit, /TEXT\("DOR_ServerFinishChoice"\)/);
});
