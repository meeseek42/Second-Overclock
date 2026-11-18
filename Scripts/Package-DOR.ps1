param([string]$PreparedInputs,[string]$OutputDirectory,[string]$StagingDirectory)
. (Join-Path $PSScriptRoot 'DOR-Paths.ps1')

if($PreparedInputs){
    . (Join-Path $PSScriptRoot 'DOR-Prepared.ps1')
    $null=Assert-DORPreparedInputs -Receipt $PreparedInputs
}else{ & (Join-Path $PSScriptRoot 'Assert-DORGameBuild.ps1') }

Assert-DORPath -LiteralPath $script:UnrealPak -Description 'UnrealPak'
Assert-DORPath -LiteralPath $script:CookedDORContent -Description 'Cooked DOR content'
Assert-DORPath -LiteralPath $script:CookedAssetRegistry -Description 'Cooked asset registry'

$staging = if($StagingDirectory){[IO.Path]::GetFullPath($StagingDirectory)}else{Join-Path $script:DORRoot 'Mod\Staging'}
$builds = if($OutputDirectory){[IO.Path]::GetFullPath($OutputDirectory)}else{Join-Path $script:DORRoot 'Builds'}
$responseFile = Join-Path $staging 'DOR-pak-response.txt'
# Historical diagnostics artifact is retained and must never be overwritten: DualOverclockReward-alpha86-apply001-diagnostics.pak
$pakFile = Join-Path $builds 'DualOverclockReward-alpha170-contributor-reviewed16-nonrelease-final.pak'
New-Item -ItemType Directory -Path $staging -Force | Out-Null
New-Item -ItemType Directory -Path $builds -Force | Out-Null
if (Test-Path -LiteralPath $pakFile) {
    throw "Refusing to overwrite existing DOR pak: $pakFile"
}

$lines = @(
    '"{0}" "../../../FSD/AssetRegistry.bin"' -f $script:CookedAssetRegistry
)
$lines += foreach ($file in Get-ChildItem -LiteralPath $script:CookedDORContent -Recurse -File | Sort-Object FullName) {
    $relative = $file.FullName.Substring($script:CookedDORContent.Length).TrimStart('\').Replace('\', '/')
    '"{0}" "../../../FSD/Content/DualOverclockReward/{1}"' -f $file.FullName, $relative
}
if ($lines.Count -le 1) {
    throw 'No cooked DOR files were found.'
}
$lines | Set-Content -LiteralPath $responseFile -Encoding utf8

& $script:UnrealPak $pakFile "-create=$responseFile" -compress
if ($LASTEXITCODE -ne 0) {
    throw "UnrealPak failed with exit code $LASTEXITCODE"
}
Assert-DORPath -LiteralPath $pakFile -Description 'DOR pak'
Write-Host "DOR pak created: $pakFile"
