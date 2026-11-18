param([string]$PreparedInputs, [ValidateSet('Inputs','Runtime','Validation','Compile','Generate')][string]$StartAt = 'Inputs', [switch]$HurricaneReadbackOnly, [switch]$BulldogCryoReadbackOnly, [switch]$SelectedSixReadbackOnly, [switch]$ConnectionReadbackOnly, [switch]$MaintenanceReadbackOnly, [switch]$MaintenanceCompileOnly, [switch]$TimeBlueprintBuild)
. (Join-Path $PSScriptRoot 'DOR-Paths.ps1')
. (Join-Path $PSScriptRoot 'DOR-Prepared.ps1')
if($PreparedInputs){
    $PreparedInputs=[IO.Path]::GetFullPath($PreparedInputs)
    $preparedRoot=Assert-DORPreparedInputs -Receipt $PreparedInputs
    $script:Manifest=Join-Path $preparedRoot 'authoring/runtime-pairs.json'
    $script:RuntimeCsv=Join-Path $preparedRoot 'authoring/runtime-pairs.csv'
    $displayPath=Join-Path $preparedRoot 'authoring/display-overrides.json'
    Assert-DORPath -LiteralPath $displayPath -Description 'Receipt-bound display input adjacent to prepared manifest'
    $preparedReceipt=Read-DORJsonFile -LiteralPath $PreparedInputs -Description 'public authoring receipt'
    $displayMember=@($preparedReceipt.members | Where-Object path -eq 'authoring/display-overrides.json')
    if($displayMember.Count -ne 1 -or (Get-DORSha256 -LiteralPath $displayPath) -ne $displayMember[0].sha256 -or (Get-DORSha256 -LiteralPath $displayPath) -ne (Get-DORSha256 -LiteralPath (Join-Path $script:DORRoot 'Mod/Generated/display-overrides.json'))){throw 'Prepared display input missing, unbound or changed; rebuild authoring before generation.'}
}
$readbackOnly = $HurricaneReadbackOnly -or $BulldogCryoReadbackOnly -or $SelectedSixReadbackOnly -or $ConnectionReadbackOnly -or $MaintenanceReadbackOnly -or $MaintenanceCompileOnly
$stageIndex = @('Inputs','Runtime','Validation','Compile','Generate').IndexOf($StartAt)
if ($readbackOnly -and $stageIndex -lt 3) { $stageIndex = 3 }

Assert-DORPath -LiteralPath $script:UEBuild -Description 'UE4 build tool'
Assert-DORPath -LiteralPath $script:UEEditorCmd -Description 'UE4 commandlet'
Assert-DORPath -LiteralPath (Join-Path $script:FSDTemplate 'FSD.uproject') -Description 'FSD project'

# Input refresh is explicit; the historical preparer emits pre-policy recipes.
# Normal builds validate the current production pins through runtime generation.
if (-not $PreparedInputs -and $stageIndex -le 0) {
    Assert-DORPath -LiteralPath (Join-Path $script:DORRoot 'Analysis\data\manual\remaining-resolved-native-inputs.json') -Description 'Resolved remaining native inputs'
}
Push-Location $script:DORRoot
try {
    $workspaceNode = $script:NodeExecutable
    if (-not (Get-Command $workspaceNode -ErrorAction SilentlyContinue)) { throw "Configured Node executable not found: $workspaceNode" }
    if (-not $PreparedInputs -and $stageIndex -le 1) {
    & $workspaceNode 'Analysis/src/cli.mjs' runtime
    if ($LASTEXITCODE -ne 0) {
        throw "Runtime manifest generation failed with exit code $LASTEXITCODE"
    }
    & $workspaceNode --input-type=module -e 'import { pinGF01Runtime } from "./Analysis/src/runtime/gf02-contract.mjs"; pinGF01Runtime();'
    if ($LASTEXITCODE -ne 0) {
        throw "GF-01 unchanged projection verification/runtime re-pin failed with exit code $LASTEXITCODE"
    }
    }
    if (-not $PreparedInputs -and $stageIndex -le 2) {
    & $workspaceNode --test --experimental-test-isolation=none 'Analysis/tests/validation/*.test.mjs'
    if ($LASTEXITCODE -ne 0) {
        throw "Current validation suite failed with exit code $LASTEXITCODE"
    }
    }
}
finally {
    Pop-Location
}

$runtimeManifest = Read-DORJsonFile -LiteralPath $script:Manifest -Description 'runtime manifest'
if ([string]::IsNullOrWhiteSpace([string]$runtimeManifest.manifestHash)) {
    throw 'Runtime manifest does not contain manifestHash.'
}
$gf01ContractPath = Join-Path $script:DORRoot 'Analysis\research\implementation-wave4-gf01\batch-contract\pair-contract.json'
if($PreparedInputs){$gf01ContractPath=Join-Path $preparedRoot 'authoring/gf01-contract.json'}
$gf01Contract = Read-DORJsonFile -LiteralPath $gf01ContractPath -Description 'GF-01 v2 contract'
if ($gf01Contract.schema -ne 'dor.gf01.batch-contract.v2' -or $gf01Contract.counts.candidate -ne 154) {
    throw 'GF-01 contract schema or candidate count changed.'
}
if(-not $PreparedInputs){
foreach ($sourceName in @('classification', 'families', 'runtime', 'implementations', 'catalog')) {
    $relativePath = [string]$gf01Contract.authority.paths.$sourceName
    $expectedHash = [string]$gf01Contract.authority.sourceHashes.$sourceName
    $stream = [System.IO.File]::OpenRead((Join-Path $script:DORRoot $relativePath))
    try {
        $sha256 = [System.Security.Cryptography.SHA256]::Create()
        $actualHash = [System.BitConverter]::ToString($sha256.ComputeHash($stream)).Replace('-', '')
    }
    finally {
        $stream.Dispose()
        if ($sha256) { $sha256.Dispose() }
    }
    if (-not $actualHash.Equals($expectedHash, [StringComparison]::OrdinalIgnoreCase)) {
        throw "GF-01 pinned source hash mismatch: $sourceName ($relativePath)"
    }
}
}
$expectedBuild = '0.93.14-alpha170-contributor-reviewed16-nonrelease'
$auditPath = Join-Path $script:FSDTemplate 'Saved\DORGraphAudit.json'
$generationStartedUtc = [DateTime]::UtcNow
if(-not $PreparedInputs){
$gf02NativeSourceContentRoot = Join-Path $script:DORRoot 'Analysis\research\alpha123-gf02-batch\current-input-extract-absolute\FSD\Content'
$gf02NativeSourceDirectories = @('WeaponsNTools', 'Game', 'GameElements')
$temporaryNativeSourceRoots = @($gf02NativeSourceDirectories | ForEach-Object {
    Join-Path $script:FSDTemplate (Join-Path 'Content' $_)
})
$generationStartedUtc = [DateTime]::UtcNow
if (-not $readbackOnly -and (Test-Path -LiteralPath $auditPath -PathType Leaf)) {
    Remove-Item -LiteralPath $auditPath -Force
}
foreach ($directory in $gf02NativeSourceDirectories) {
    $sourceRoot = Join-Path $gf02NativeSourceContentRoot $directory
    $temporaryRoot = Join-Path $script:FSDTemplate (Join-Path 'Content' $directory)
    Assert-DORPath -LiteralPath $sourceRoot -Description "Resolved GF02 native source tree ($directory)"
    if (Test-Path -LiteralPath $temporaryRoot) {
        throw "Refusing to stage GF02 native source over an existing template tree: $temporaryRoot"
    }
    Copy-Item -LiteralPath $sourceRoot -Destination $temporaryRoot -Recurse
}

$gf04SourceRoot = Join-Path $script:DORRoot 'Analysis\research\alpha125-gf04-batch\current-input-extract\FSD\Content\WeaponsNTools'
Copy-Item -Path (Join-Path $gf04SourceRoot '*') -Destination (Join-Path $script:FSDTemplate 'Content\WeaponsNTools') -Recurse -Force
$remainingSourceRoot = Join-Path $script:DORRoot 'Analysis\research\alpha128-native-bounce-sticky\current-input-extract\FSD\Content\WeaponsNTools'
Copy-Item -Path (Join-Path $remainingSourceRoot '*') -Destination (Join-Path $script:FSDTemplate 'Content\WeaponsNTools') -Recurse -Force
$combinedSourceRoot = Join-Path $script:DORRoot 'Analysis\research\native-combined13-plan-20261016\current-extract\FSD\Content\WeaponsNTools'
Copy-Item -Path (Join-Path $combinedSourceRoot '*') -Destination (Join-Path $script:FSDTemplate 'Content\WeaponsNTools') -Recurse -Force
$salvoSourceRoot = Join-Path $script:DORRoot 'Analysis\research\fusion-classification\global-audit\human-opinion-review\numeric-proposals\revision-current-extract\FSD\Content\WeaponsNTools\MicroMissileLauncher'
foreach ($name in @('PRJ_MicroMissile','PRJ_MicroMissile_Buck','PRJ_MicroMissile_Cluster','BP_ClusterFragment_MicroMissileLauncher','PRJ_MicroMissile_Mine_Head','PRJ_MicroMissile_PlasmaBurster','CRV_MML_Acceleration','CRV_MML_InstantAcceleration')) {
    foreach ($extension in @('.uasset','.uexp','.ubulk')) {
        $source = Join-Path $salvoSourceRoot ($name + $extension)
        if (Test-Path -LiteralPath $source) { Copy-Item -LiteralPath $source -Destination (Join-Path $script:FSDTemplate ('Content\WeaponsNTools\MicroMissileLauncher\' + $name + $extension)) -Force }
    }
}
$gf03SourceRoot = Join-Path $script:DORRoot 'Analysis\research\alpha126-gf03-batch\current-input-extract\FSD\Content\WeaponsNTools'
Copy-Item -Path (Join-Path $gf03SourceRoot '*') -Destination (Join-Path $script:FSDTemplate 'Content\WeaponsNTools') -Recurse -Force
$dualSourceRoot = Join-Path $script:DORRoot 'Analysis\research\bulldog-cryo-deeper-routes-20261019\current-extract\FSD\Content'
$dualSemanticRoot = Join-Path $script:DORRoot 'Analysis\research\alpha140-bulldog-cryo\semantic-extract\FSD\Content'
$dualTemporaryFiles = @()
foreach ($root in @($dualSourceRoot,$dualSemanticRoot)) {
    foreach ($source in Get-ChildItem -LiteralPath $root -Filter '*.uasset' -Recurse -File | Where-Object { $_.BaseName -notlike 'WPN_*' }) {
        $relative = $source.FullName.Substring($root.Length + 1)
        $target = Join-Path (Join-Path $script:FSDTemplate 'Content') $relative
        New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
        foreach ($extension in @('.uasset','.uexp','.ubulk')) {
            $file = [IO.Path]::ChangeExtension($source.FullName,$extension)
            if (Test-Path -LiteralPath $file) {
                $destination = [IO.Path]::ChangeExtension($target,$extension)
                if ($relative.StartsWith('Audio\',[StringComparison]::OrdinalIgnoreCase)) {
                    if (Test-Path -LiteralPath $destination) { throw "Refusing to replace an existing Audio asset: $destination" }
                    $dualTemporaryFiles += $destination
                }
                Copy-Item -LiteralPath $file -Destination $destination -Force
            }
        }
    }
}
$sludgeSourceRoot = Join-Path $script:DORRoot 'Analysis\research\sludge-five-native-boundary-20261018\current-extract\FSD\Content\WeaponsNTools\GooCannon'
foreach ($source in Get-ChildItem -LiteralPath $sludgeSourceRoot -Filter '*.uasset' -Recurse -File | Where-Object { $_.FullName.Contains('\Overclocks\') -or $_.BaseName -match '^(PRJ_|STE_ExplosiveGoo|BP_GooExplosion|BP_GooCannon_GooPuddle)' }) {
    $relative = $source.FullName.Substring($sludgeSourceRoot.Length + 1)
    $target = Join-Path (Join-Path $script:FSDTemplate 'Content\WeaponsNTools\GooCannon') $relative
    New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
    foreach ($extension in @('.uasset','.uexp','.ubulk')) {
        $sourceFile = [IO.Path]::ChangeExtension($source.FullName,$extension)
        if (Test-Path -LiteralPath $sourceFile) { Copy-Item -LiteralPath $sourceFile -Destination ([IO.Path]::ChangeExtension($target,$extension)) -Force }
    }
}

}

try {
    if($PreparedInputs){$preparedStagedFiles=Add-DORPreparedNativeContent -PreparedRoot $preparedRoot -Phase generate}
    Sync-DORSource
    if (-not $readbackOnly) { Clear-DORManagedOutput -Root $script:TemplateDORContent -ManifestPath $script:TemplateManagedContent }
    Invoke-WithDORDrives {
        if ($stageIndex -le 3) {
        & $script:UEBuild FSDEditor Win64 Development 'R:\FSD.uproject' -WaitMutex -NoHotReload -NoDebugInfo -MaxParallelActions=4
        if ($LASTEXITCODE -ne 0) {
            throw "DOREditor build failed with exit code $LASTEXITCODE"
        }
        }
        $readbackArgument = @()
        if ($HurricaneReadbackOnly) { $readbackArgument = @('-HurricaneReadbackOnly') }
        if ($BulldogCryoReadbackOnly) { $readbackArgument = @('-BulldogCryoReadbackOnly') }
        if ($SelectedSixReadbackOnly) { $readbackArgument = @('-SelectedSixReadbackOnly') }
        if ($ConnectionReadbackOnly) { $readbackArgument = @('-ConnectionReadbackOnly') }
        if ($MaintenanceCompileOnly) { $readbackArgument = @('-MaintenanceCompileOnly') }
        if ($MaintenanceReadbackOnly) { $readbackArgument = @('-MaintenanceReadbackOnly') }
        if ($TimeBlueprintBuild) { $readbackArgument += '-TimeBlueprintBuild' }
        $contractArgs=@("-Manifest=$script:Manifest","-Csv=$script:RuntimeCsv","-GF01Contract=$gf01ContractPath")
        foreach($family in @('GF02','GF04','GF03','Remaining')){
            if($PreparedInputs){$contract=Join-Path $preparedRoot ('authoring/'+$family.ToLower()+'-recipes.json');$evidence=Join-Path $preparedRoot ('authoring/'+$family.ToLower()+'-resolved-native-inputs.json')}
            else{$contract=Join-Path $script:DORRoot ('Analysis/data/manual/'+$family.ToLower()+'-recipes.json');$evidence=Join-Path $script:DORRoot ('Analysis/data/manual/'+$family.ToLower()+'-resolved-native-inputs.json')}
            $contractArgs+=@("-$($family)Contract=$contract","-$($family)ResolvedInputs=$evidence")
        }
        if($PreparedInputs){
            $null=Assert-DORPreparedInputs -Receipt $PreparedInputs
            $contractArgs+=@("-PreparedInputs=$PreparedInputs","-PreparedSourceRoot=$script:DORRoot")
        }
        & $script:UEEditorCmd 'R:\FSD.uproject' -run=DORGenerateAssets -DOR165AmmoAuthoring @readbackArgument @contractArgs -unattended -nop4 -NullRHI -NoSplash
        if ($LASTEXITCODE -ne 0) {
            throw "DOR asset generation failed with exit code $LASTEXITCODE"
        }
    }
}
catch {
    # A failed full build still owns its generated namespace. Retain its exact
    # inventory so the next normal build clears these partial outputs too.
    if(-not $readbackOnly -and (Test-Path -LiteralPath $script:TemplateDORContent)){
        Write-DORManagedInventory -Root $script:TemplateDORContent -ManifestPath $script:TemplateManagedContent
    }
    throw
}
finally {
    if($PreparedInputs){Remove-DORPreparedNativeContent -Files $preparedStagedFiles}
    else {
    foreach ($file in $dualTemporaryFiles) {
        $resolved = [IO.Path]::GetFullPath($file)
        $audioRoot = [IO.Path]::GetFullPath((Join-Path $script:FSDTemplate 'Content\Audio')).TrimEnd('\') + '\'
        if (-not $resolved.StartsWith($audioRoot,[StringComparison]::OrdinalIgnoreCase)) { throw "Audio staging cleanup escaped workspace Content: $resolved" }
        if (Test-Path -LiteralPath $resolved) { Remove-Item -LiteralPath $resolved -Force }
    }
    foreach ($temporaryRoot in $temporaryNativeSourceRoots) {
        if (Test-Path -LiteralPath $temporaryRoot) {
            $resolved = [IO.Path]::GetFullPath($temporaryRoot)
            $contentRoot = [IO.Path]::GetFullPath((Join-Path $script:FSDTemplate 'Content')).TrimEnd('\') + '\'
            if (-not $resolved.StartsWith($contentRoot, [StringComparison]::OrdinalIgnoreCase)) { throw "Native staging cleanup escaped workspace Content: $resolved" }
            Remove-Item -LiteralPath $resolved -Recurse -Force
        }
    }
}

}

if ($MaintenanceCompileOnly) { Write-Host 'Maintenance adapters compiled in memory; no generated or saved asset updates.'; return }
if ($readbackOnly) { Write-Host 'Existing saved assets checked in a fresh editor process.'; return }

Assert-DORPath -LiteralPath $script:TemplateDORContent -Description 'Generated DOR content'
Assert-DORPath -LiteralPath $auditPath -Description 'Current DOR graph audit'
$auditItem = Get-Item -LiteralPath $auditPath
if ($auditItem.LastWriteTimeUtc -lt $generationStartedUtc.AddSeconds(-2)) {
    throw "DOR graph audit is stale: $auditPath"
}
$audit = Read-DORJsonFile -LiteralPath $auditPath -Description 'current DOR graph audit'
if ($audit.schema -ne 2 -or -not $audit.passed) {
    throw 'DOR graph audit did not pass the current schema.'
}
if ($audit.build -ne $expectedBuild) {
    throw "DOR graph audit build mismatch: expected '$expectedBuild', got '$($audit.build)'."
}
if ($audit.manifestHash -ne $runtimeManifest.manifestHash) {
    throw 'DOR graph audit manifest hash does not match the generated runtime manifest.'
}
if ($audit.generatorSource -ne 'DORGenerateAssetsCommandlet.cpp+DORModHub.inl') {
    throw "DOR graph audit provenance is unexpected: $($audit.generatorSource)"
}
Write-DORManagedInventory -Root $script:TemplateDORContent -ManifestPath $script:TemplateManagedContent
Sync-DORGeneratedContentToMod
Write-Host 'DOR assets generated and synchronized.'
