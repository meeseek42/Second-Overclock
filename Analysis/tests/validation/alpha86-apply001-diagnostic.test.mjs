import test from "node:test";
import assert from "node:assert/strict";
import fs from "node:fs";

const source = fs.readFileSync(new URL(
  "../../../Mod/Source/DOREditor/Private/DORGenerateAssetsCommandlet.cpp",
  import.meta.url,
), "utf8");
const apply = source.slice(
  source.indexOf("bool BuildApplyPairFunction"),
  source.indexOf("bool BuildApplyOriginalFunction"),
);

test("current APPLY-001 diagnostic field schema remains available", () => {
  for (const field of [
    "stage=APPLY-001", "pair", "category", "HasFiveTiers", "IsPairValid",
    "WeaponMatches", "equippedCount", "pairLeafCount", "currentItem",
    "currentClass", "expectedActorClass", "mappedLoadoutClass",
    "playerValid", "inventoryValid", "itemValid",
  ]) {
    assert.ok(apply.includes(field), `missing ${field}`);
  }
});
