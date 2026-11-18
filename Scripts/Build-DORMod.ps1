param([string]$PreparedInputs,[string]$EvidenceDirectory,[string]$OutputDirectory,[string]$BaselineEvidenceDirectory,[string]$BaselineBuildTag,[string]$BaselineManifestHash,[ValidateSet('Generate','SavedReadback','Cook','CookedReadback','Package')][string]$StartAt='Generate')
$ErrorActionPreference = 'Stop'
if(-not $PreparedInputs){
    & (Join-Path $PSScriptRoot 'Assert-DORGameBuild.ps1')
    & (Join-Path $PSScriptRoot 'Generate-DORAssets.ps1')
    & (Join-Path $PSScriptRoot 'Cook-DORAssets.ps1')
    & (Join-Path $PSScriptRoot 'Build-DORWeaponMirrors.ps1')
    & (Join-Path $PSScriptRoot 'Package-DOR.ps1')
    return
}
. (Join-Path $PSScriptRoot 'DOR-Paths.ps1')
. (Join-Path $PSScriptRoot 'DOR-Prepared.ps1')
. (Join-Path $PSScriptRoot 'DOR-BuildEvidence.ps1')
$PreparedInputs=[IO.Path]::GetFullPath($PreparedInputs)
$preparedRoot=Assert-DORPreparedInputs -Receipt $PreparedInputs
$script:Manifest=Join-Path $preparedRoot 'authoring/runtime-pairs.json'
$script:RuntimeCsv=Join-Path $preparedRoot 'authoring/runtime-pairs.csv'
$identityModule=(Join-Path $script:DORRoot 'Analysis/tests/validation/current-artifact-identity.mjs').Replace('\','/')
$buildTag=& $script:NodeExecutable --input-type=module -e 'import {pathToFileURL} from "node:url"; console.log((await import(pathToFileURL(process.argv[1]))).ACCEPTED_BUILD_TAG);' $identityModule
if($LASTEXITCODE -ne 0 -or -not $buildTag){throw 'Build identity lookup failed.'}
if(-not $EvidenceDirectory){$EvidenceDirectory=Join-Path $script:DORRoot ('.local/build/'+$buildTag)}
$EvidenceDirectory=[IO.Path]::GetFullPath($EvidenceDirectory)
if(-not $OutputDirectory){$OutputDirectory=Join-Path $EvidenceDirectory 'artifacts'}
if($StartAt -eq 'Generate' -and (Test-Path -LiteralPath $EvidenceDirectory)){throw 'Use a fresh evidence directory for full generation.'}
New-Item -ItemType Directory -Path $EvidenceDirectory -Force | Out-Null
$frozen=Join-Path $EvidenceDirectory 'pre-cook-inputs.json'
$timings=[System.Collections.Generic.List[object]]::new()
if($StartAt -ne 'Generate' -and (Test-Path -LiteralPath (Join-Path $EvidenceDirectory 'stage-times.json'))){foreach($row in @(Read-DORJsonFile -LiteralPath (Join-Path $EvidenceDirectory 'stage-times.json') -Description 'prior stage timings')){$timings.Add($row)}}
function Invoke-DORStage([string]$Name,[scriptblock]$Action){
    $timer=[Diagnostics.Stopwatch]::StartNew()
    try{& $Action}finally{$timer.Stop();$timings.Add([pscustomobject]@{stage=$Name;seconds=$timer.Elapsed.TotalSeconds});$timings | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $EvidenceDirectory 'stage-times.json') -Encoding utf8}
}
function Read-DORCandidate([string]$Kind,[string]$Base){
    & (Join-Path $PSScriptRoot 'Read-DORBuild.ps1') -AssetRoot $Base -EvidenceDirectory $EvidenceDirectory -Kind $Kind -BuildTag $buildTag
    & $script:NodeExecutable (Join-Path $PSScriptRoot 'Check-DORBuildReadback.mjs') $Kind (Join-Path $EvidenceDirectory "$Kind-inspect") $buildTag $EvidenceDirectory $PreparedInputs $BaselineEvidenceDirectory $BaselineBuildTag $BaselineManifestHash
    if($LASTEXITCODE -ne 0){throw "$Kind full serialized readback failed"}
    Assert-DORReadbackEvidence -Directory $EvidenceDirectory -Kind $Kind -BuildTag $buildTag
}
if($StartAt -eq 'Generate'){
    & (Join-Path $PSScriptRoot 'Initialize-DORBuildTemplate.ps1') -TemplateDirectory $script:FSDTemplate -ReceiptPath (Join-Path $EvidenceDirectory 'template-receipt.json')
    Invoke-DORStage 'Generate' {& (Join-Path $PSScriptRoot 'Generate-DORAssets.ps1') -PreparedInputs $PreparedInputs -TimeBlueprintBuild *> (Join-Path $EvidenceDirectory 'generate.log')}
    Copy-Item -LiteralPath (Join-Path $script:FSDTemplate 'Saved/DORNativeStage-generate.json') -Destination (Join-Path $EvidenceDirectory 'native-generate-stage.json')
    Copy-Item -LiteralPath (Join-Path $script:FSDTemplate 'Saved/DORGraphAudit.json') -Destination (Join-Path $EvidenceDirectory 'DORGraphAudit.json')
    & $script:NodeExecutable (Join-Path $PSScriptRoot 'Check-DORGeneration.mjs') $EvidenceDirectory $buildTag
    if($LASTEXITCODE -ne 0){throw 'Generation timing/count/audit closure failed before Cook'}
}
if($StartAt -eq 'SavedReadback'){
    & $script:NodeExecutable (Join-Path $PSScriptRoot 'Check-DORGeneration.mjs') $EvidenceDirectory $buildTag
    if($LASTEXITCODE -ne 0){throw 'Cannot resume without successful generation evidence'}
}
if($StartAt -in @('Generate','SavedReadback')){
    Invoke-DORStage 'SavedReadback' {Read-DORCandidate 'saved' $script:DORContent}
    Write-DORFrozenBuildInputs -Filename $frozen -PreparedInputs $PreparedInputs -BuildTag $buildTag
}
if($StartAt -in @('Generate','SavedReadback','Cook')){
    Assert-DORReadbackEvidence -Directory $EvidenceDirectory -Kind 'saved' -BuildTag $buildTag
    Assert-DORFrozenBuildInputs -Filename $frozen -BuildTag $buildTag
    Invoke-DORStage 'Cook' {& (Join-Path $PSScriptRoot 'Cook-DORAssets.ps1') -PreparedInputs $PreparedInputs -ReferenceEvidenceRoot (Join-Path $EvidenceDirectory 'cook-reference') *> (Join-Path $EvidenceDirectory 'cook.log')}
    Copy-Item -LiteralPath (Join-Path $script:FSDTemplate 'Saved/DORNativeStage-cook.json') -Destination (Join-Path $EvidenceDirectory 'native-cook-stage.json')
    Invoke-DORStage 'Mirror' {& (Join-Path $PSScriptRoot 'Build-DORWeaponMirrors.ps1') -PreparedInputs $PreparedInputs *> (Join-Path $EvidenceDirectory 'mirror.log')}
}
if($StartAt -ne 'Package'){
    Assert-DORFrozenBuildInputs -Filename $frozen -BuildTag $buildTag
    Invoke-DORStage 'CookedReadback' {Read-DORCandidate 'cooked' $script:CookedDORContent}
}
Assert-DORFrozenBuildInputs -Filename $frozen -BuildTag $buildTag
Assert-DORReadbackEvidence -Directory $EvidenceDirectory -Kind 'saved' -BuildTag $buildTag
Assert-DORReadbackEvidence -Directory $EvidenceDirectory -Kind 'cooked' -BuildTag $buildTag
Invoke-DORStage 'Package' {
    & (Join-Path $PSScriptRoot 'Package-DOR.ps1') -PreparedInputs $PreparedInputs -OutputDirectory $OutputDirectory -StagingDirectory (Join-Path $EvidenceDirectory 'staging') *> (Join-Path $EvidenceDirectory 'pak-create.log')
    Copy-Item -LiteralPath (Join-Path $EvidenceDirectory 'staging/DOR-pak-response.txt') -Destination (Join-Path $EvidenceDirectory 'pak-response.txt')
    $pak=Join-Path $OutputDirectory ('DualOverclockReward-'+$buildTag.Substring($buildTag.IndexOf('alpha'))+'-final.pak')
    foreach($action in @('Verify','List')){& $script:UnrealPak $pak "-$action" *> (Join-Path $EvidenceDirectory "pak-$($action.ToLowerInvariant()).log");if($LASTEXITCODE -ne 0){throw "PAK $action failed"}}
    $copy=Join-Path $EvidenceDirectory 'member-proof.pak';$extract=Join-Path $EvidenceDirectory 'member-proof'
    if((Test-Path -LiteralPath $copy) -or (Test-Path -LiteralPath $extract)){throw 'Disposable member proof already exists.'}
    Copy-Item -LiteralPath $pak -Destination $copy
    & $script:UnrealPak $copy -Extract $extract *> (Join-Path $EvidenceDirectory 'pak-extract-copy.log')
    if($LASTEXITCODE -ne 0){throw 'Disposable PAK copy extraction failed'}
    & $script:NodeExecutable (Join-Path $PSScriptRoot 'Check-DORPackage.mjs') $pak $extract $EvidenceDirectory $buildTag
    if($LASTEXITCODE -ne 0){throw 'Exact package membership/byte proof failed'}
}
