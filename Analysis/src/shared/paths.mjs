import path from "node:path";
import fs from "node:fs";
import { fileURLToPath } from "node:url";

const sharedDir = path.dirname(fileURLToPath(import.meta.url));

export const analysisDir = path.resolve(sharedDir, "..", "..");
export const projectDir = path.resolve(analysisDir, "..");
export const configDir = path.join(analysisDir, "config");
export const sourceDataDir = path.join(analysisDir, "data", "source");
export const derivedDataDir = path.join(analysisDir, "data", "derived");
export const manualDataDir = path.join(analysisDir, "data", "manual");
export const runtimeDataDir = path.join(analysisDir, "data", "runtime");
export const generatedDir = path.join(analysisDir, "generated");
export const modDir = path.join(projectDir, "Mod");
export const modGeneratedDir = path.join(modDir, "Generated");
export const cacheDir = path.join(analysisDir, ".cache");
export const extractedDir = path.join(cacheDir, "ExtractedAssets");
export const parsedDir = path.join(cacheDir, "ParsedAssets");
export const inspectorProjectDir = path.join(analysisDir, "tools", "UAssetInspector");
export const inspectorDll = path.join(
  inspectorProjectDir,
  "bin",
  "Release",
  "net8.0",
  "UAssetInspector.dll",
);

export const files = {
  catalog: path.join(sourceDataDir, "overclock-catalog.json"),
  gameBuild: path.join(configDir, "game-build.json"),
  gamePaths: path.join(configDir, "game-paths.local.json"),
  weaponMirrors: path.join(configDir, "weapon-mirrors.vertical.json"),
  packageManifest: path.join(derivedDataDir, "package-manifest.json"),
  parsedIndex: path.join(derivedDataDir, "parsed-assets-index.json"),
  implementations: path.join(derivedDataDir, "overclock-implementations.json"),
  validationQueue: path.join(derivedDataDir, "runtime-validation-queue.json"),
  pairAssessments: path.join(derivedDataDir, "pair-assessments.json"),
  runtimeManifest: path.join(modGeneratedDir, "runtime-pairs.json"),
  compatibilityResearchManifest: path.join(modGeneratedDir, "compatibility-research.json"),
  runtimePairsCsv: path.join(modGeneratedDir, "runtime-pairs.csv"),
  runtimeTestIndexCsv: path.join(generatedDir, "runtime", "pair-test-index.csv"),
  compatibilityResearchDir: path.join(generatedDir, "research"),
  implementationOverrides: path.join(manualDataDir, "implementation-overrides.json"),
  pairOverrides: path.join(manualDataDir, "pair-overrides.json"),
  numericStackPolicy: path.join(manualDataDir, "numeric-stack-policy.json"),
  runtimeEvidence: path.join(runtimeDataDir, "runtime-evidence.json")
};

// Read-only relocation for byte-pinned private inputs. Never rewrite recorded input bytes.
export const localInputsRoot = process.env.DOR_INPUTS_ROOT ? path.resolve(process.env.DOR_INPUTS_ROOT) : projectDir;
export function resolveInput(filename) {
 const name = filename instanceof URL ? fileURLToPath(filename) : String(filename);
 const normalized = name.replaceAll('\\','/');
 let relative = path.relative(projectDir,name).replaceAll('\\','/');
 if (!relative.startsWith('../') && !path.isAbsolute(relative)) {
   const local = path.join(localInputsRoot,relative);
   if (localInputsRoot !== projectDir && fs.existsSync(local) && !fs.existsSync(name)) return local;
   return name;
 }
 const marker = normalized.indexOf('/Analysis/');
 if (marker >= 0) return path.join(localInputsRoot,normalized.slice(marker+1));
 throw Error('Unmapped recorded input path: '+name);
}

// Historical contracts pin original loader bytes as evidence. Execute the maintained
// loader, but verify those immutable provenance bytes from the supplied snapshot.
export function resolveEvidence(filename) {
 const relative = path.relative(projectDir, filename);
 if (relative.startsWith('..') || path.isAbsolute(relative)) throw Error('Evidence path escapes project');
 const local = path.join(localInputsRoot, relative);
 return localInputsRoot !== projectDir && fs.existsSync(local) ? local : resolveInput(filename);
}
