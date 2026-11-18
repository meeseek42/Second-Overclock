import test from "node:test";
import assert from "node:assert/strict";
import fs from "node:fs";

const source = fs.readFileSync(new URL(
  "../../../Mod/Source/DOREditor/Private/DORGenerateAssetsCommandlet.cpp",
  import.meta.url,
), "utf8");
const continuation = source.slice(
  source.indexOf('TEXT("DOR_ContinuePair388PresetTransaction"),\n        UEdGraph::StaticClass()'),
  source.indexOf('TEXT("DOR_PairContinuationTick"),\n        UEdGraph::StaticClass()'),
);
const watchdog = source.slice(
  source.indexOf('TEXT("DOR_PairContinuationTick"),\n        UEdGraph::StaticClass()'),
  source.indexOf("bool AddPresetTransactionEndPlayGuard"),
);

test("current legacy continuation retains its pending-token false assignment", () => {
  assert.match(continuation, /TEXT\("Pair388ContinuationPending"\)\)->DefaultValue = TEXT\("false"\)/);
});
