import { extractGame } from "./extract/extract-game.mjs";
import { parseAssets } from "./parse/parse-assets.mjs";
import { buildContracts } from "./contracts/build-contracts.mjs";
import { validateContracts } from "./contracts/validate-contracts.mjs";
import { assessPairs } from "./assess/assess-pairs.mjs";
import { buildReports } from "./reports/build-reports.mjs";
import { buildRuntimeManifest } from "./runtime/build-runtime-manifest.mjs";
import { buildCompatibilityResearch } from "./research/build-compatibility-research.mjs";

const command = process.argv[2];

const commands = {
  extract: extractGame,
  parse: parseAssets,
  contracts: buildContracts,
  "validate-contracts": validateContracts,
  assess: assessPairs,
  reports: buildReports,
  runtime: buildRuntimeManifest,
  research: () => {
    buildRuntimeManifest();
    return buildCompatibilityResearch();
  },
  all: async () => {
    await extractGame();
    await parseAssets();
    await buildContracts();
    validateContracts();
    assessPairs();
    buildRuntimeManifest();
    buildCompatibilityResearch();
    await buildReports();
  }
};

if (!commands[command]) {
  throw new Error(`Unknown command "${command}". Expected one of: ${Object.keys(commands).join(", ")}`);
}

const result = await commands[command]();
if (result?.metadata) console.log(JSON.stringify(result.metadata, null, 2));
