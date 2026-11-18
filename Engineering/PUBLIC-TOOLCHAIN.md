# Public prepared authoring route

T1 compiles public authored data into generator contracts and runtime inputs. T2 prepares current original source data using an explicit versioned expectation profile. The public route does not read historical private catalogs, parsed indexes, implementation snapshots or research files. Legacy mode retains its existing byte pins and semantics.

## Source compatibility and evidence

Whole PAK/Shipping EXE fingerprints are **informational provenance**. Admission checks the actual consumed target exports: source class, serialized fields and serialized archetype chain, root GUID/order/category, component ownership and carrier relationships. A historical member hash difference triggers semantic validation; it is not an automatic rejection. The pinned DRAK mirror triple retains its six serialization hashes because the mirror tool depends on those exact bytes.

Absent fields retain their established recipe expectations and provenance with explicit pending loaded-UObject equality checks. The 309 formerly blocked leaf entries represent 29 class/field/value groups; the 112 historically supported entries represent four groups. They are not independent current Shipping observations. Preparation and editor checks do **not** certify current Shipping constructor behavior. Existing C++ loaded-object checks remain mandatory and stop on a disagreement.

The actual fresh local run prepared 5,828 packages / 11,725 original Content members / 255,956,592 bytes, using 47 inspector batches. It verified 491 source records,160 roots and carrier facts, retaining422 pending loaded-object field checks. This is the observed transitive hard-import closure, **not a minimality claim**. Audio, UI and character dependencies are included where hard imports require them. The source expectations did not need reclassification. The clean contributor editor/build route is validated below; Shipping gameplay and constructor behavior are not certified by that evidence.

## Commands

From this source snapshot, use independently obtained tools and originals:

```powershell
.\Scripts\Prepare-DORNativeInputs.ps1 -GamePak <original.pak> -ShippingExe <original-Shipping.exe> -UnrealPak <UnrealPak.exe> -Dotnet <dotnet.exe> -Inspector <built-UAssetInspector.dll> -OutputDirectory .local/prepared/current
.\Scripts\Build-DORAuthoring.ps1 -SourceReceipt .local/prepared/current/source-receipt.json
```

Fresh preparation hashes actual originals, passes one disposable PAK copy to `UnrealPak -Extract <directory>`, and selects original roots/leaves plus maintained authoring/template/component/carrier/static dependencies. It expands actual hard package imports and preserves existing companions. UObject identities use case-insensitive lookup while maintained authored paths stay fixed. Missing required packages identify their consumer. Receipts use canonical package/member identities and are written last.

Ten explicit ownership entries prevent generated stubs, validator test literals and the runtime-only DropPod soft-class expectation from becoming original package seeds. Dynamic authoring selectors use the same ownership rules. The dependency inventory distinguishes these expectations from staged originals and marks the mirror source triple.

To prepare a second private output from a verified cache, add `-ReuseReceipt <completed source-receipt.json>`. Cache reuse verifies the complete member closure, profile, actual original-source provenance and semantic validator/inspector identities. Changed whole-source provenance requires fresh preparation for cache freshness; it does not make that source version incompatible. The cache path performs no extraction or inspector launches. Changed/missing prepared members always fail integrity checks.

## Supported author edit

The first supported surface is optional `outputOverride` on an existing emitted numeric leaf. For GF02 code1 in `Authoring/recipes/gf02.json`:

```json
"outputOverride": { "value": 5, "reason": "contributor balance experiment" }
```

The adapter retains native amounts3/1 and computed selection4, then emits effective float32 Amount5. Do not manually update selections, serialized results, source amounts, IDs, GUIDs, paths or order. GF03 operation/generated-leaf duplicates join by output path. Integer/preservation operations, unsupported fields/classes, nonfinite/overflowing values and negative percentage factors reject. Removing the override reproduces the baseline.

`runtime-base.json` retains all456 ordered descriptors, including seven non-recipe rows and259 mechanism metadata. Recipe numeric arrays are rebuilt through the existing overlays. The authoring receipt binds actual inputs, prepared source receipt, contracts/evidence, runtime outputs and generator/adapter source. Native evidence distinguishes actual EXE provenance from the recipe's historical expectation basis. Neither value is substituted for the other.

Generate, Cook, Mirror and Build-Mod accept explicit `-PreparedInputs <.../authoring/prepared-inputs.json>`. Generate uses the prepared contracts/evidence/runtime, and Generate/Cook stage only phase-selected inventoried native files with conflict refusal and cleanup of newly staged files. The full original provenance cache is retained. Mirror uses the prepared DRAK triple. The commandlet verifies bytes before contract parsing, then retains actual loaded-object semantic guards. Node checks SHA256 and UE4.27 Core checks companion SHA1 byte digests. Both Generate and Cook retain `-DOR165AmmoAuthoring`; all23 world/Actor fixtures remain skipped.

## Clean build and maintained evidence route

The independently fetched `DRG-Modding/FSD-Template` commit `15fef14b2c20ce25d11b60346f0b7f8db500d7e3`, tree `7b3fae7864fa2186057ef8790f122553d34dcc84`, contains8,850 tracked Source/Plugins entries and no tracked Content. DOREditor-only project/target integration compiles with UE4.27.2 and the installed C++ toolchain. Both public .NET tools build. No old local template binary/content or unrelated TBBREditor module is required.

```powershell
# Obtain the upstream checkout independently, at the exact commit above.
.\Scripts\Initialize-DORBuildTemplate.ps1 -TemplateDirectory <template> -ReceiptPath .local/template-receipt.json
.\Scripts\Build-DORMod.ps1 -PreparedInputs <prepared/authoring/prepared-inputs.json> -EvidenceDirectory .local/build/<unique-run> -OutputDirectory .local/build/<unique-run>/artifacts
```

Configure the same template with Setup-DORTools. Initialization verifies pinned source/plugin inputs and DOREditor-only integration. Reuse permits only declared compiler-generated Win64 editor binary/module/target overlays, whose identities are recorded; other tracked changes reject. The legacy route remains available without PreparedInputs.

`Authoring/native-editor-staging.json` assigns canonical packages to Generate, Cook, mirror-only, runtime-reference-only or provenance-only ownership. The observed preparation retained5,828 packages /11,725 original members. Generate selects833 packages /1,666 members; Cook selects71 /142. This reproduces the editor consumers without mounting the entire cooked hard-import closure. Eight MicroMissile projectile/curve seeds and explicit copied Cryo particle/audio dependencies remain. Runtime VFX imports/assets are not rewritten, substituted or generically filtered by extension. All selected bytes are receipt-backed originals. Missing Niagara warnings can occur as in the established editor route; the resulting Hurricane children retain their original superclass/CDO and copied movement fields. Readback additionally compares those serialized parent identities and copied component templates, including Cryo.

Prepared authoring includes the maintained `display-overrides.json` beside its manifest; receipt and early preflight verify exact source bytes. The static generator input inventory found12 required prepared inputs and no other adjacent configuration input. Generation and Cook both retain `-DOR165AmmoAuthoring`; all23 world/Actor fixtures remain skipped.

Build-Mod performs normal full Generate with compile timing, count/audit closure, saved readback, frozen source/runtime/generated inventories, one Cook with reference checks, Mirror, final cooked readback and exact package member/byte verification. Resume points `SavedReadback`, `Cook`, `CookedReadback` and `Package` preserve preceding evidence and require applicable current byte/frozen-input checks; they avoid repeating a successful expensive phase after a validation-tool repair. `-BaselineEvidenceDirectory`, `-BaselineBuildTag` and `-BaselineManifestHash` enable saved-to-saved and cooked-to-cooked comparison. Baseline captures are optional external evidence, not normal build prerequisites.

Readback compares emitted signatures/flags, resolved calls, statement destinations, native/CDO defaults, struct schemas and focused inheritance/templates. Compiler-generated localization/struct/latent identities are decoded by their exact serialized roles, retaining text, field schema, latent linkage and identity reuse relationships. Context skip sizes are verified against the decoded expression size; ubergraph entry offsets resolve to statement targets. Exact supplied build/manifest identities and declared numeric overrides are the only authored differences normalized. Original rejected reports and frozen-input repair records remain evidence; arbitrary GUIDs, numbers and properties are not ignored.

## Observed nonrelease proof

The public candidate `0.93.14-alpha170-contributor-reviewed16-nonrelease` completed the maintained route using the installed-source receipt and independently pinned template. Full generation measured1,337.376557s, with1,295 explicit compile calls /1,232.267786s: Master443 and Safety185, matching the reviewed nine-plus-seven deferrals from452/192. All1,672 saves and three retained audits passed. Clean C++ compilation had passed separately; this generation's incremental UBT measured3.22s. These are observed timings, not a controlled attribution of elapsed-time improvement to the16 deferrals.

Saved and cooked readbacks each cover1,203 assets and ten containment groups, with1,050 saved /1,067 cooked emitted functions. They match preserved accepted169 semantics after the stated decoding and explicit version/manifest/Amount5 differences. The GF02 code1 `DORMerged_8798251d6822b038_01` reads Amount5 in both kinds; native3/1 and computed4 remain, and all other455 runtime rows are unchanged. Cook/reference checks measured72.3096823s; Mirror8.1899575s. Final package verification covers2,407 exact members and extracted-copy bytes. The test PAK is3,752,714 bytes, SHA256 `df34299503b4a42468a46a3a11eec796393c55cbc58d3537cb540670e8f60780`; it is a local nonrelease experiment, excluded from the public payload. No mod.io ZIP or publication was made.

The delivered public recipe has the override removed and baseline authoring rebuilt. That restored baseline was not regenerated through UE again. Editor/build evidence does not claim Shipping gameplay execution. No game test is needed solely for compile-scheduling equivalence.
