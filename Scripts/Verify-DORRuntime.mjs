import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { buildRuntimeManifest } from '../Analysis/src/runtime/build-runtime-manifest.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const outputRoot = path.join(root, '.local/runtime');
fs.mkdirSync(outputRoot, { recursive: true });
const output = fs.mkdtempSync(path.join(outputRoot, 'check-'));
const manifest = buildRuntimeManifest({ runtimeManifest: path.join(output, 'runtime-pairs.json'), runtimePairsCsv: path.join(output, 'runtime-pairs.csv'), runtimeTestIndexCsv: path.join(output, 'pair-test-index.csv') });
// Versioned advance after reproducing v1 with unchanged isolated private inputs,
// then comparing every field except the intentional259 metadata and hashes.
// The historical pin is retained in Authoring/manifest.json; private authorities
// are unchanged. This is not the future public authoring/provenance adapter.
const baselineVersion = 'dor.contributor.runtime-baseline.v2';
const previous = 'a49387dd59331a3c952e2d500d28e64776d0564dffd010d3494c7c72005aacc6';
const expected = '284ddf933a931813e52a71ac36e32ca0150a1dc07b66e6e206f6c53e4101ee0c';
if (manifest.manifestHash !== expected || manifest.pairs.length !== 456) throw Error('Baseline runtime identity changed; do not repin. Inspect private output: ' + output);
console.log(JSON.stringify({ passed: true, baselineVersion, previousManifestHash: previous, catalogPairs: manifest.pairs.length, manifestHash: manifest.manifestHash, output, coverage: 'Relocated Node runtime generation only; no Unreal build or gameplay validation' }, null, 2));
