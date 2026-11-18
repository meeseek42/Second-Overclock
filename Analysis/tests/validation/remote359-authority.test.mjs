import test from "node:test";
import assert from "node:assert/strict";
import fs from "node:fs";

const read = (path) => fs.readFileSync(new URL(path, import.meta.url), "utf8");
const probe = read("../../../Mod/Source/DOREditor/Private/DORRemote359.inl");

// These check generated-graph wiring contracts, not native setter/replication behavior.
test("current remote359 refused outcomes retain false publication fields", () => {
  assert.match(probe, /RemoteLastSucceeded"\),nullptr,TEXT\("false"\)/);
  assert.match(probe, /PendingReplayRecord"\),nullptr,TEXT\("false"\)/);
});
