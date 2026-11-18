import fs from "node:fs";
import path from "node:path";
import { spawnSync } from "node:child_process";
import {
  files,
  inspectorDll,
  inspectorProjectDir,
  parsedDir
} from "../shared/paths.mjs";
import { exists, readJson, sha256File, stableId, uniqueSorted, writeJson } from "../shared/io.mjs";

function packagePathFromObjectPath(objectPath) {
  return objectPath.split(".")[0];
}

function collectReferences(value, output = new Set()) {
  if (typeof value === "string") {
    const matches = value.match(/\/Game\/[A-Za-z0-9_./+-]+/g) ?? [];
    for (const match of matches) output.add(match.split(".")[0].replace(/[)"',]+$/, ""));
  } else if (Array.isArray(value)) {
    for (const item of value) collectReferences(item, output);
  } else if (value && typeof value === "object") {
    for (const item of Object.values(value)) collectReferences(item, output);
  }
  return output;
}

function shouldFollow(packagePath) {
  if (packagePath.startsWith("/Game/WeaponsNTools/")) return true;
  return /(?:Weapon|Projectile|Upgrade|Damage|Status|Effect|Component|Mine|Puddle|Trail|Cryo|Fire|Heat|Cold|Explosion)/i.test(
    packagePath,
  );
}

export function runInspector(dotnet, list, outputDirectory, options = {}) {
  if (!fs.existsSync(outputDirectory)) {
    throw new Error(`Inspector output directory does not exist: ${outputDirectory}`);
  }
  if (fs.readdirSync(outputDirectory).length !== 0) {
    throw new Error(`Inspector output directory is not fresh: ${outputDirectory}`);
  }
  const listPath = path.join(outputDirectory, "parse-batch.txt");
  fs.writeFileSync(listPath, `${list.join("\n")}\n`, "utf8");
  const inspectorPath = options.inspectorPath ?? inspectorDll;
  const spawnEnvironment = options.env
    ? { ...process.env, ...options.env }
    : undefined;
  if (!fs.existsSync(inspectorPath)) {
    const build = spawnSync(
      dotnet,
      ["build", inspectorProjectDir, "-c", "Release"],
      { encoding: "utf8", maxBuffer: 64 * 1024 * 1024, windowsHide: true },
    );
    if (build.status !== 0) throw new Error(`UAssetInspector build failed.\n${build.stdout}\n${build.stderr}`);
  }
  const result = spawnSync(
    dotnet,
    [inspectorPath, listPath, outputDirectory],
    {
      encoding: "utf8",
      maxBuffer: 64 * 1024 * 1024,
      windowsHide: true,
      env: spawnEnvironment,
    },
  );
  if (result.error) throw new Error(`UAssetInspector failed to start: ${result.error.message}`);
  if (result.signal) {
    throw new Error(`UAssetInspector terminated by signal ${result.signal}.\n${result.stdout}\n${result.stderr}`);
  }
  if (result.status !== 0) {
    throw new Error(`UAssetInspector failed with exit code ${result.status}.\n${result.stdout}\n${result.stderr}`);
  }
  const batchResult = path.join(outputDirectory, "batch-result.json");
  if (!fs.existsSync(batchResult)) {
    throw new Error(`UAssetInspector produced no batch result.\n${result.stdout}\n${result.stderr}`);
  }
  const batch = (options.readResult ?? readJson)(batchResult);
  if (!Array.isArray(batch.results)) throw new Error("UAssetInspector batch result has no results array.");
  const expected = new Map(list.map((input) => [path.resolve(input).toLowerCase(), input]));
  if (expected.size !== list.length) throw new Error("Inspector input list contains duplicate paths.");
  const seen = new Set();
  for (const row of batch.results) {
    if (!row || typeof row.input !== "string") throw new Error("Inspector result is missing its input path.");
    const key = path.resolve(row.input).toLowerCase();
    if (!expected.has(key)) throw new Error(`Inspector returned an unexpected input: ${row.input}`);
    if (seen.has(key)) throw new Error(`Inspector returned duplicate input: ${row.input}`);
    seen.add(key);
  }
  if (seen.size !== expected.size) {
    const missing = [...expected.keys()].filter((key) => !seen.has(key));
    throw new Error(`Inspector omitted requested inputs: ${missing.join(", ")}`);
  }
  return batch;
}

export async function parseAssets() {
  if (!exists(files.packageManifest)) throw new Error("Run extract before parse.");
  const catalog = readJson(files.catalog);
  const manifest = readJson(files.packageManifest);
  const paths = readJson(files.gamePaths);
  const packageByPath = new Map(manifest.packages.map((row) => [row.packagePath, row]));

  const roots = catalog.weapons.flatMap((weapon) =>
    weapon.overclocks.map((overclock) => packagePathFromObjectPath(overclock.objectPath)),
  );
  const queue = [...uniqueSorted(roots)];
  const visited = new Set();
  const rows = [];

  while (queue.length > 0) {
    const batchPackages = [];
    while (queue.length > 0 && batchPackages.length < 128) {
      const packagePath = queue.shift();
      if (visited.has(packagePath)) continue;
      visited.add(packagePath);
      const manifestRow = packageByPath.get(packagePath);
      if (manifestRow) batchPackages.push({ packagePath, ...manifestRow });
      else rows.push({ packagePath, success: false, error: "package-not-found" });
    }
    if (batchPackages.length === 0) continue;

    const batchDirectory = fs.mkdtempSync(path.join(parsedDir, "batch-"));
    const result = runInspector(paths.dotnet, batchPackages.map((row) => row.uasset), batchDirectory);
    const packageByInput = new Map(batchPackages.map((row) => [path.resolve(row.uasset).toLowerCase(), row]));

    for (const parsed of result.results) {
      const packageRow = packageByInput.get(path.resolve(parsed.input).toLowerCase());
      if (!packageRow) continue;
      if (!parsed.success) {
        rows.push({
          packagePath: packageRow.packagePath,
          success: false,
          error: parsed.error,
          uasset: packageRow.uasset
        });
        continue;
      }
      const document = readJson(parsed.output);
      const references = uniqueSorted([...collectReferences(document)]).filter(shouldFollow);
      for (const reference of references) {
        if (!visited.has(reference) && packageByPath.has(reference)) queue.push(reference);
      }
      rows.push({
        packagePath: packageRow.packagePath,
        success: true,
        uasset: packageRow.uasset,
        uassetSha256: await sha256File(packageRow.uasset),
        parsedJson: parsed.output,
        references
      });
    }
  }

  rows.sort((a, b) => a.packagePath.localeCompare(b.packagePath, "en"));
  const output = {
    schema: "dual-overclock-reward.parsed-assets-index.v1",
    metadata: {
      rootCount: roots.length,
      packageCount: rows.length,
      successCount: rows.filter((row) => row.success).length,
      failureCount: rows.filter((row) => !row.success).length,
      parser: "UAssetAPI 1.1.0 / UE4.27",
      scoringTextInference: false
    },
    rows
  };
  writeJson(files.parsedIndex, output);
  return output;
}
