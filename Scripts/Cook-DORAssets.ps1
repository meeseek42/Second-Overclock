param([string]$ReferenceEvidenceRoot,[string]$PreparedInputs)

. (Join-Path $PSScriptRoot 'DOR-Paths.ps1')
. (Join-Path $PSScriptRoot 'DOR-Prepared.ps1')
if($PreparedInputs){$preparedRoot=Assert-DORPreparedInputs -Receipt $PreparedInputs}

Assert-DORPath -LiteralPath $script:UEEditorCmd -Description 'UE4 commandlet'
Assert-DORPath -LiteralPath (Join-Path $script:FSDTemplate 'FSD.uproject') -Description 'FSD project'
Sync-DORContentToTemplate

# These hard references must resolve while Cook reloads the generated leaves.
# Package-DOR only includes the DOR namespace; these original inputs are never packed.
if(-not $PreparedInputs){
$nativeSourceContent = Join-Path $script:DORRoot 'Analysis\research\alpha123-gf02-batch\current-input-extract-absolute\FSD\Content'
$requiredNativePackages = @(
    'GameElements/Damage/DMG_Cold',
    'WeaponsNTools/MicroMissileLauncher/PRJ_MicroMissile',
    'WeaponsNTools/_UpgradeCategories/UPC_ProjectileSpeed'
)
$gf03SourceContent = Join-Path $script:DORRoot 'Analysis\research\alpha126-gf03-batch\current-input-extract\FSD\Content'
$requiredNativePackages += @('WeaponsNTools/ChargeBlaster/PRJ_ChargedBlasterShot', 'WeaponsNTools/PlasmaCarbine/PRJ_PlasmaCarbineShot')
$requiredNativePackages += @(Get-ChildItem -LiteralPath (Join-Path $gf03SourceContent 'WeaponsNTools\GooCannon') -Filter 'PRJ_*.uasset' -File | ForEach-Object { 'WeaponsNTools/GooCannon/' + $_.BaseName })
$sludgeSourceContent = Join-Path $script:DORRoot 'Analysis\research\sludge-five-native-boundary-20261018\current-extract\FSD\Content'
$sludgeSourceFolder = Join-Path $sludgeSourceContent 'WeaponsNTools\GooCannon'
$requiredNativePackages += @(Get-ChildItem -LiteralPath $sludgeSourceFolder -Filter '*.uasset' -File -Recurse | Where-Object { $_.FullName.Contains('\Overclocks\') -or $_.BaseName -match '^(PRJ_|STE_ExplosiveGoo|BP_GooExplosion|BP_GooCannon_GooPuddle)' } | ForEach-Object { 'WeaponsNTools/GooCannon/' + $_.FullName.Substring($sludgeSourceFolder.Length + 1).Replace('\','/').Replace('.uasset','') })
$salvoSourceContent = Join-Path $script:DORRoot 'Analysis\research\fusion-classification\global-audit\human-opinion-review\numeric-proposals\revision-current-extract\FSD\Content'
$requiredNativePackages += @('PRJ_MicroMissile','PRJ_MicroMissile_Buck','PRJ_MicroMissile_Cluster','BP_ClusterFragment_MicroMissileLauncher','PRJ_MicroMissile_Mine_Head','PRJ_MicroMissile_PlasmaBurster','CRV_MML_Acceleration','CRV_MML_InstantAcceleration' | ForEach-Object { 'WeaponsNTools/MicroMissileLauncher/' + $_ })
$requiredNativePackages += 'WeaponsNTools/LockOnRifle/BP_LockOnRifleAutoFireComponent'
$dualSourceContent = Join-Path $script:DORRoot 'Analysis\research\bulldog-cryo-deeper-routes-20261019\current-extract\FSD\Content'
$dualSemanticContent = Join-Path $script:DORRoot 'Analysis\research\alpha140-bulldog-cryo\semantic-extract\FSD\Content'
$requiredNativePackages += @('WeaponsNTools/Cryospray/PRJ_Cryocannon','WeaponsNTools/Cryospray/PRJ_Crye_SnowBall','WeaponsNTools/Cryospray/STE_Snowball_LingeringCold','WeaponsNTools/Cryospray/Particles/P_Snowball_Explosion_Cryocannon','Audio/SFX/WeaponsNTools/WPN_CryoSpray/SnowballExplosion_Cryocannon_Cue')
$requiredNativePackages = @($requiredNativePackages | Select-Object -Unique)
}
$stagedNativeFiles = [System.Collections.Generic.List[string]]::new()
$createdNativeDirectories = [System.Collections.Generic.List[string]]::new()
$templateContent = [IO.Path]::GetFullPath((Join-Path $script:FSDTemplate 'Content')).TrimEnd('\') + '\'
try {
    if($PreparedInputs){$preparedStagedFiles=Add-DORPreparedNativeContent -PreparedRoot $preparedRoot -Phase cook}
    else {
    foreach ($relativePackage in $requiredNativePackages) {
        foreach ($extension in @('.uasset', '.uexp', '.ubulk')) {
            $relativeFile = $relativePackage + $extension
            $sourceFile = Join-Path $nativeSourceContent $relativeFile
            $gf03SourceFile = Join-Path $gf03SourceContent $relativeFile
            if (Test-Path -LiteralPath $gf03SourceFile -PathType Leaf) { $sourceFile = $gf03SourceFile }
            $sludgeSourceFile = Join-Path $sludgeSourceContent $relativeFile
            if (Test-Path -LiteralPath $sludgeSourceFile -PathType Leaf) { $sourceFile = $sludgeSourceFile }
            $salvoSourceFile = Join-Path $salvoSourceContent $relativeFile
            if ($relativePackage.StartsWith('WeaponsNTools/MicroMissileLauncher/') -and (Test-Path -LiteralPath $salvoSourceFile -PathType Leaf)) { $sourceFile = $salvoSourceFile }
            foreach ($sourceRoot in @($dualSourceContent,$dualSemanticContent)) {
                $dualSourceFile = Join-Path $sourceRoot $relativeFile
                if (Test-Path -LiteralPath $dualSourceFile -PathType Leaf) { $sourceFile = $dualSourceFile }
            }
            if ($extension -eq '.uasset') {
                Assert-DORPath -LiteralPath $sourceFile -Description "Required native Cook reference ($relativePackage)"
            }
            if (-not (Test-Path -LiteralPath $sourceFile -PathType Leaf)) { continue }
            $targetFile = [IO.Path]::GetFullPath((Join-Path $templateContent $relativeFile))
            if (-not $targetFile.StartsWith($templateContent, [StringComparison]::OrdinalIgnoreCase)) {
                throw "Native Cook reference escapes template Content: $targetFile"
            }
            if (Test-Path -LiteralPath $targetFile) {
                if ((Get-FileHash -LiteralPath $targetFile).Hash -ne (Get-FileHash -LiteralPath $sourceFile).Hash) {
                    throw "Conflicting existing native Cook reference: $targetFile"
                }
                continue
            }
            $parent = Split-Path -Parent $targetFile
            while (-not (Test-Path -LiteralPath $parent)) {
                if (-not $parent.StartsWith($templateContent, [StringComparison]::OrdinalIgnoreCase)) {
                    throw "Native Cook reference parent escapes template Content: $parent"
                }
                if (-not $createdNativeDirectories.Contains($parent)) { $createdNativeDirectories.Add($parent) }
                $parent = Split-Path -Parent $parent
            }
            New-Item -ItemType Directory -Path (Split-Path -Parent $targetFile) -Force | Out-Null
            $stagedNativeFiles.Add($targetFile)
            Copy-Item -LiteralPath $sourceFile -Destination $targetFile
        }
    }

    }
Invoke-WithDORDrives {
    $userRoot = 'R:\Saved\DORUEUser'
    $shaderDir = 'R:\Saved\DORShaderWorkingDir'
    New-Item -ItemType Directory -Path $userRoot -Force | Out-Null
    New-Item -ItemType Directory -Path $shaderDir -Force | Out-Null
    $previousUserProfile = $env:USERPROFILE
    $previousAppData = $env:APPDATA
    $previousLocalAppData = $env:LOCALAPPDATA
    $previousTemp = $env:TEMP
    $previousTmp = $env:TMP
    try {
        $env:USERPROFILE = $userRoot
        $env:APPDATA = Join-Path $userRoot 'AppData\Roaming'
        $env:LOCALAPPDATA = Join-Path $userRoot 'AppData\Local'
        $env:TEMP = Join-Path $userRoot 'Temp'
        $env:TMP = $env:TEMP
        New-Item -ItemType Directory -Path $env:APPDATA -Force | Out-Null
        New-Item -ItemType Directory -Path $env:LOCALAPPDATA -Force | Out-Null
        New-Item -ItemType Directory -Path $env:TEMP -Force | Out-Null
        & $script:UEEditorCmd 'R:\FSD.uproject' -run=cook -DOR165AmmoAuthoring `
            -targetplatform=WindowsNoEditor `
            -CookDir=/Game/DualOverclockReward `
            -unattended -nop4 -NullRHI -NoSplash `
            "-ShaderWorkingDir=$shaderDir"
        if ($LASTEXITCODE -ne 0) {
            throw "DOR cook failed with exit code $LASTEXITCODE"
        }
    }
    finally {
        $env:USERPROFILE = $previousUserProfile
        $env:APPDATA = $previousAppData
        $env:LOCALAPPDATA = $previousLocalAppData
        $env:TEMP = $previousTemp
        $env:TMP = $previousTmp
    }
}

Assert-DORPath -LiteralPath $script:CookedDORContent -Description 'Cooked DOR content'
& (Join-Path $PSScriptRoot 'Assert-DORCookReferences.ps1') -EvidenceDirectory $ReferenceEvidenceRoot
Write-Host "DOR content cooked: $script:CookedDORContent"
}
finally {
    if($PreparedInputs){Remove-DORPreparedNativeContent -Files $preparedStagedFiles}
    foreach ($targetFile in $stagedNativeFiles) {
        if (Test-Path -LiteralPath $targetFile -PathType Leaf) { Remove-Item -LiteralPath $targetFile -Force }
    }
    foreach ($directory in ($createdNativeDirectories | Sort-Object Length -Descending)) {
        if ((Test-Path -LiteralPath $directory -PathType Container) -and
            -not (Get-ChildItem -LiteralPath $directory -Force | Select-Object -First 1)) {
            Remove-Item -LiteralPath $directory -Force
        }
    }
}
