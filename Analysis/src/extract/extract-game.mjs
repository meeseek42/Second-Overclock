import fs from "node:fs";
import path from "node:path";
import { spawn } from "node:child_process";
import { files, extractedDir } from "../shared/paths.mjs";
import { readJson, sha256File, writeJson } from "../shared/io.mjs";

function run(filename, args) {
  return new Promise((resolve, reject) => {
    const child = spawn(filename, args, { stdio: "inherit", windowsHide: true });
    child.on("error", reject);
    child.on("exit", (code) => {
      if (code === 0) resolve();
      else reject(new Error(`${path.basename(filename)} exited with code ${code}`));
    });
  });
}

function packagePathFromFilename(filename) {
  const normalized = filename.replaceAll("\\", "/");
  const marker = "/FSD/Content/";
  const index = normalized.indexOf(marker);
  if (index < 0 || !normalized.endsWith(".uasset")) return null;
  return `/Game/${normalized.slice(index + marker.length, -".uasset".length)}`;
}

function walk(directory, rows = []) {
  for (const entry of fs.readdirSync(directory, { withFileTypes: true })) {
    const filename = path.join(directory, entry.name);
    if (entry.isDirectory()) walk(filename, rows);
    else if (entry.name.endsWith(".uasset")) {
      const packagePath = packagePathFromFilename(filename);
      if (!packagePath) continue;
      const uexp = filename.slice(0, -".uasset".length) + ".uexp";
      rows.push({
        packagePath,
        uasset: filename,
        uexp: fs.existsSync(uexp) ? uexp : null,
        uassetBytes: fs.statSync(filename).size,
        uexpBytes: fs.existsSync(uexp) ? fs.statSync(uexp).size : 0
      });
    }
  }
  return rows;
}

export async function extractGame() {
  const paths = readJson(files.gamePaths);
  const catalog = readJson(files.catalog);
  for (const [label, filename] of [["gamePak", paths.gamePak], ["unrealPak", paths.unrealPak]]) {
    if (!fs.existsSync(filename)) throw new Error(`${label} not found: ${filename}`);
  }

  fs.mkdirSync(extractedDir, { recursive: true });
  for (const folder of [...new Set(catalog.weapons.map((weapon) => weapon.assetFolder))].sort()) {
    for (const directory of ["Overclocks", "OCs"]) {
      const filter = `FSD/Content/WeaponsNTools/${folder}/${directory}/*`;
      await run(paths.unrealPak, [paths.gamePak, "-Extract", extractedDir, `-Filter=${filter}`]);
    }
  }

  const pakSha256 = await sha256File(paths.gamePak);
  const build = readJson(files.gameBuild);
  writeJson(files.gameBuild, {
    ...build,
    pakSha256,
    extractedAt: new Date().toISOString()
  });

  const packages = walk(extractedDir).sort((a, b) => a.packagePath.localeCompare(b.packagePath, "en"));
  const manifest = {
    schema: "dual-overclock-reward.package-manifest.v1",
    metadata: {
      gameBuild: build.build,
      pakSha256,
      packageCount: packages.length
    },
    packages
  };
  writeJson(files.packageManifest, manifest);
  return manifest;
}
