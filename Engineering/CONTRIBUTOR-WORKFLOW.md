# Engineering entry points

This 0.93.14 snapshot includes a maintained contributor route from public authored data and independently supplied compatible originals/tools to a verified test PAK. Clean upstream integration, current-source preparation and the supported numeric edit have passed Generate, saved readback, Cook, Mirror, cooked readback and exact package checks. External UE/template/C++/.NET tools and original game inputs remain prerequisites. See [PUBLIC-TOOLCHAIN.md](PUBLIC-TOOLCHAIN.md) and [TOOL-GAPS.md](TOOL-GAPS.md) for exact commands and evidence limits.

## File responsibilities

| Location | Maintained purpose |
| --- | --- |
| `Authoring/manifest.json` | Authoring surface index, schemas, baseline and integration status |
| `Authoring/recipes/{gf02,gf03,gf04,remaining}.json` | 294 recipes: ordered occurrences, operations, selection/formula/rounding policy, serialized numerical results, templates, root/schematic/leaf identities, attachments and component/carrier rules |
| `Authoring/gf01-pairs.json` | 155 GF01 pair contracts, stable identity/order/admission decisions; no raw route evidence |
| `Authoring/stable-identities.json` | 456 runtime pair IDs/codes in their current order, paths and available contract GUIDs; never regenerate codes by sorting names. Generator-only legacy GUID formulas remain in C++ |
| `Authoring/native-editor-staging.json` | Canonical phase-specific editor consumers and runtime/provenance ownership; preserves full original cache |
| `Authoring/admission.json` | 453 admitted entries within the 456-row catalog; admission is distinct from gameplay confirmation |
| `Authoring/classification.json`, `families.json` | Current classification decisions and family definitions; original human/AI discussions and raw evidence excluded. Classification verification descriptions are not current gameplay confirmations |
| `Authoring/overclock-identities.json` | 160 stable OC selectors, category/identity and ordered native-leaf expectations; no raw exports |
| `Analysis/data/manual/*-overrides.json`, `numeric-stack-policy.json`, `high-load-policy.json` | Pair/implementation decisions, numerical rules and 37 default-off high-load exclusions |
| `Mod/Generated/display-overrides.json` | Maintained 24-weapon/160-OC names and translated effects. Despite its directory name, this is an input |
| `Analysis/config/weapon-mirrors.vertical.json` | Audited native mirror mapping and compatible source hashes |
| `Mod/Source/DOREditor` | Blueprint/asset generation, native graph checks and runtime behavior |
| `Analysis/src/runtime`, `Analysis/tests/validation` | Legacy runtime generation and current focused checks; some checks require prepared private inputs |
| `Analysis/tools`, `Scripts` | Maintained .NET inspector/mirror sources and setup/build utilities |

All 294 recipe projections and 456 ordered stable identities remain current. Recipe projections retain every numerical, leaf/occurrence/order, operation, stable ID, policy and native expectation field. Only historical authority metadata, raw evidence references in classification/route metadata and stale classification verification metadata were removed. Full legacy input bytes remain private; no private authority hashes were repinned. Projection schemas are versioned separately from the legacy contracts. Schema adaptation, numeric output override validation and explicit prepared-input generator hooks are implemented; real-source preparation and supported Amount5 saved/cooked propagation pass.

Mechanism augmentation has separate ownership: runtime259's `rj250Double` metadata names its baseline, component and mechanism leaf. The numerical recipe still validates its original nine leaves; C++ appends the tenth mechanism leaf. This runtime/C++ behavior is included, but is not yet an editable public recipe field. Mechanism fields remain immutable in the first public editing surface.

## Tool setup

Use Windows, PowerShell 7, Node 22 or later and a .NET SDK that supports `net8.0`. UE generation additionally requires UE 4.27 and an independently supplied compatible FSD template and C++ toolchain. No external binaries or engine/template source are included.

From this repository root, run:

```powershell
pwsh -NoProfile -File Scripts/Setup-DORTools.ps1 -NodeExecutable node -DotnetExecutable dotnet
```

To select installed tools and a separately prepared template, add `-UE427Root '<UE_4.27 directory>' -FSDTemplateRoot '<independent FSD template directory>'`. The script restores and builds both .NET projects, using `.local/nuget/packages` and `.local/dotnet-home`. It writes `.local/tool-paths.json` only after both builds succeed; omitted UE/template values retain any existing configuration. `-NuGetSource '<explicit local feed or URL>'` selects a separately supplied package source. Default restore uses only the official NuGet source in `NuGet.Config`; no user cache is silently substituted.

Legacy PowerShell build scripts read that local configuration (or `DOR_TOOLS_CONFIG`). `UE427_ROOT` is an optional UE fallback. An occupied R: or S: drive causes a failure before drive substitution; existing mappings are never removed. Initialize-DORBuildTemplate verifies the supported clean upstream pin and integration.

## Transitional private snapshot check

This route requires a separately supplied private snapshot **and its exact SHA256/size manifest**. It is not a local-game bootstrap, and the public tree intentionally supplies neither. The manifest uses `{ "schema": 1, "files": [{ "path": "project/relative/file", "bytes": 123, "sha256": "64 hex digits" }] }`. The legacy Remaining contract also pins original loader bytes as provenance: its original `Analysis/src/runtime/remaining-contract.mjs` must be present in that private snapshot, separate from the maintained loader being executed.

```powershell
node Scripts/Import-DORInputSnapshot.mjs '<private snapshot root>' '<private manifest.json>' '<new private inputs directory>'
$env:DOR_INPUTS_ROOT = '<new private inputs directory>'
node Scripts/Verify-DORRuntime.mjs
```

The importer rejects unsafe/duplicate paths, changed bytes and existing destinations. A completed receipt is written last. The verifier writes only ignored `.local/runtime/check-*` output and requires 456 rows with current baseline v2 hash `284ddf933a931813e52a71ac36e32ca0150a1dc07b66e6e206f6c53e4101ee0c`. `Authoring/manifest.json` retains historical v1 hash `a49387dd59331a3c952e2d500d28e64776d0564dffd010d3494c7c72005aacc6` and the explicit migration record. Before advancing the verifier, the same isolated historical private inputs reproduced v1, then current generation matched v2 exactly; the only differences were runtime259's `rj250Double` metadata, its row hash and the manifest hash. Every other field, including identity/admission/order/numerical leaves, compared equal. No private authority bytes changed.

A missing parsed category is an error. Recorded parsed paths are remapped at read time; input bytes are not rewritten. This verifies Node runtime generation, not C++/Cook/gameplay or T1 public editing. `DOR_INPUTS_ROOT` is supported for this runtime check only; legacy Unreal scripts still require their prepared project-relative overlay.

## Build boundary

`npm run build:mod` is the preserved legacy Generate → Cook → Mirror → Package route. It requires the private project-relative legacy overlay, a separately integrated template, restored tools and compatible local native source assets. It is **not a fresh setup command**. Historical `all` and historical native preparers are absent from package scripts because they can rewrite accepted authorities or reproduce only old subsets. Do not repin hashes merely to pass validation.

The historical game configuration records CL148500 and a specific PAK hash; the legacy game gate warns and continues on mismatch. This is insufficient for fresh input acceptance. The new explicit prepared route admits consumed source semantics and records actual whole-file fingerprints as provenance. Absent native fields remain established expectations for deferred loaded-object checks. Real fresh preparation is locally checked. The pinned clean upstream T3 route passes through exact test-package verification. The old local template contains unrelated TBBREditor integration and must not be treated as a clean upstream pin.

Package output refuses to overwrite an existing filename. Use an isolated working copy and output area for any future build experiment. Native Actor/world fixtures (23) remain skipped. No game launch or save operation is part of these setup/runtime checks.

Generation and Cook both pass `-DOR165AmmoAuthoring`. `DOREditor.cpp` temporarily exposes `OnMaxAmmoCapacityChanged`, `OnTotalAmountChanged`, `OnClipAmountChanged` and `OnClipAndTotalChanged` on `ClipBasedItemAggregator`, then restores only flags it added. Preserve that exact authoring scope; widening flags or suppressing compile errors does not establish compatibility. Current Generate also supports connection/maintenance readbacks, maintenance compile-only inspection and `-TimeBlueprintBuild`.

The maintained public Build-DORMod route now invokes saved/final-cooked serialized readback, frozen source/runtime/generated semantic inventory checks and exact package member/byte verification. The public route passes on the independently pinned template using canonical Generate/Cook stage selections. Its local nonrelease170 proof includes Amount5 saved/cooked propagation and exact package bytes; see PUBLIC-TOOLCHAIN.md. Preserved accepted169 captures supply optional same-kind comparison evidence. The full hard-import cache remains provenance and is not indiscriminately mounted in the editor.

## Fork build identity

Choose a unique build tag for a fork and update these four places together before generating or packaging:

1. `DORResearchBuildTag` in `Mod/Source/DOREditor/Private/DORGenerateAssetsCommandlet.cpp`.
2. `$expectedBuild` in `Scripts/Generate-DORAssets.ps1`.
3. `$pakFile` in `Scripts/Package-DOR.ps1`, using a fresh filename.
4. `ACCEPTED_BUILD_TAG` in `Analysis/tests/validation/current-artifact-identity.mjs`; its derived package filename must match item 3. The current filename convention uses the suffix starting at `alpha`, so retain a unique `alpha...` suffix or deliberately update and validate that derivation.

The public source candidate identity is `0.93.14-alpha170-contributor-reviewed16-nonrelease`; accepted169 release artifacts remain separate and unchanged. Keep the exact version checks in `DORConnectionProtocol.inl`: different build tags are intentionally incompatible in the connection handshake. All participants in a test must use the same fork build. Do not disable compatibility or identity checks to connect unlike builds. Build tags and runtime input baselines are separate identities; a tag update does not authorize changing provenance pins.

## Public boundary

Only files in `Engineering/public-files.json` form the prepared publication payload. Generated runtime output, native assets/exports/binaries, engine/template, packages/caches, `.local/`, original classification discussions, private snapshot manifests, player data/logs, old bug history and build results are excluded. Native object paths/numbers in maintained recipes and curated expectations are retained intentionally. This is a technical boundary, not a redistribution-rights or license determination.

README languages, license, known-issue wording and feedback wording are reserved for the project owner and are not authored here.
