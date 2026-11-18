import { files } from "../shared/paths.mjs";
import { readJson } from "../shared/io.mjs";

export function validateContracts() {
  const data = readJson(files.implementations);
  const catalog = readJson(files.catalog);
  const expected = catalog.weapons.reduce((sum, weapon) => sum + weapon.overclocks.length, 0);
  const errors = [];
  if (catalog.weapons.length !== 24) errors.push(`weapon-count:${catalog.weapons.length}`);
  if (expected !== 160) errors.push(`catalog-overclock-count:${expected}`);
  if (data.overclocks.length !== 160) errors.push(`implementation-count:${data.overclocks.length}`);
  const ids = new Set(data.overclocks.map((row) => row.overclockId));
  if (ids.size !== data.overclocks.length) errors.push("duplicate-overclock-id");
  for (const row of data.overclocks) {
    if (!row.claims.length) errors.push(`${row.overclockId}:no-claims`);
    if (!row.rootSha256) errors.push(`${row.overclockId}:missing-root-hash`);
    if (row.claims.some((claim) => !claim.evidenceRefs?.length)) errors.push(`${row.overclockId}:claim-without-evidence`);
  }
  if (errors.length) throw new Error(`Contract validation failed:\n${errors.join("\n")}`);
  return {
    schema: "dual-overclock-reward.contract-validation.v1",
    metadata: { valid: true, weaponCount: 24, overclockCount: 160 },
  };
}
