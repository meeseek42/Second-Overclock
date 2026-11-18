import assert from "node:assert/strict";
import fs from "node:fs";
import test from "node:test";
import { files } from "../../src/shared/paths.mjs";
import { readJson } from "../../src/shared/io.mjs";

test("vertical cooked weapon mirror config is pinned to pair 388", () => {
  const config = readJson(files.weaponMirrors);
  assert.equal(config.schema, "dual-overclock-reward.weapon-mirrors.v1");
  assert.equal(config.verticalOnly, true);
  assert.equal(config.weapons.length, 1);

  const mirror = config.weapons[0];
  assert.equal(mirror.weapon, "PlasmaCarbine");
  assert.equal(mirror.testIndex, 388);
  assert.equal(mirror.pairId, "a2edc4fa1d61b86f");
  assert.match(mirror.mirrorSavegameId, /^[0-9A-F]{8}(?:-[0-9A-F]{4}){3}-[0-9A-F]{12}$/);
  for (const key of [
    "sourceWeaponUassetSha256",
    "sourceWeaponUexpSha256",
    "sourceBankUassetSha256",
    "sourceBankUexpSha256",
    "sourceItemIdUassetSha256",
    "sourceItemIdUexpSha256",
  ]) {
    assert.match(mirror[key], /^[0-9a-f]{64}$/);
  }
  assert.match(mirror.mirrorWeaponPackage, /^\/Game\/DualOverclockReward\/Weapons\//);
  assert.match(mirror.mirrorBankPackage, /^\/Game\/DualOverclockReward\/Weapons\//);
  assert.match(mirror.mirrorItemIdPath, /^\/Game\/DualOverclockReward\/Weapons\//);
  assert.match(mirror.nativeItemIdPath, /^\/Game\/WeaponsNTools\//);
  assert.equal(mirror.weaponItemIdMode, "native");
  assert.ok(fs.existsSync(new URL("../../tools/CookedWeaponMirror/Program.cs", import.meta.url)));
});

test("current packaging excludes native overrides and preserves guard policy", () => {
  const packageScript = fs.readFileSync(
    new URL("../../../Scripts/Package-DOR.ps1", import.meta.url),
    "utf8",
  );
  assert.doesNotMatch(packageScript, /NativeOverrides/);
  assert.doesNotMatch(packageScript, /Content[\\/]UI/);
  assert.match(packageScript, /Assert-DORGameBuild/);

  const buildScript = fs.readFileSync(
    new URL("../../../Scripts/Build-DORMod.ps1", import.meta.url),
    "utf8",
  );
  assert.doesNotMatch(buildScript, /Build-DORNativeOcOverride/);
  assert.match(buildScript, /Build-DORWeaponMirrors/);
  assert.match(buildScript, /Assert-DORGameBuild/);
  assert.doesNotMatch(buildScript, /\$LASTEXITCODE/);

  const buildGuard = fs.readFileSync(
    new URL("../../../Scripts/Assert-DORGameBuild.ps1", import.meta.url),
    "utf8",
  );
  assert.match(buildGuard, /DOR-W-GAME-BUILD-MISMATCH/);
  assert.match(buildGuard, /Write-Warning/);
  assert.match(buildGuard, /Security\.Cryptography\.SHA256/);
  assert.match(buildGuard, /IO\.File.*OpenRead/);
  assert.match(buildGuard, /pakSha256/);
});
