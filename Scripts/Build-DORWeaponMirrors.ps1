param([string]$PreparedInputs)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'DOR-Paths.ps1')
. (Join-Path $PSScriptRoot 'DOR-Prepared.ps1')

$dotnet = $script:DotnetExecutable
$toolProject = Join-Path $script:DORRoot 'Analysis\tools\CookedWeaponMirror\CookedWeaponMirror.csproj'
$toolDll = Join-Path $script:DORRoot 'Analysis\tools\CookedWeaponMirror\bin\Release\net8.0\CookedWeaponMirror.dll'
$config = Join-Path $script:DORRoot 'Analysis\config\weapon-mirrors.vertical.json'
$sourceWeapon = Join-Path $script:DORRoot 'Analysis\.cache\CurrentBuildMirrorSourceCL148500\FSD\Content\WeaponsNTools\PlasmaCarbine\WPN_PlasmaCarbine.uasset'
$sourceBank = Join-Path $script:DORRoot 'Analysis\.cache\ExtractedAssets\FSD\Content\WeaponsNTools\PlasmaCarbine\Overclocks\OSB_PlasmaCarbine.uasset'
$sourceItemId = Join-Path $script:DORRoot 'Analysis\.cache\CurrentBuildMirrorSourceCL148500\FSD\Content\WeaponsNTools\PlasmaCarbine\ID_PlasmaCarbine.uasset'
if($PreparedInputs){
    $preparedRoot=Assert-DORPreparedInputs -Receipt $PreparedInputs
    $sourceWeapon=Join-Path $preparedRoot 'Content/WeaponsNTools/PlasmaCarbine/WPN_PlasmaCarbine.uasset'
    $sourceBank=Join-Path $preparedRoot 'Content/WeaponsNTools/PlasmaCarbine/Overclocks/OSB_PlasmaCarbine.uasset'
    $sourceItemId=Join-Path $preparedRoot 'Content/WeaponsNTools/PlasmaCarbine/ID_PlasmaCarbine.uasset'
}
$cookedContent = Split-Path -Parent $script:CookedDORContent
$result = Join-Path $script:DORRoot 'Analysis\data\derived\weapon-mirror-manifest.vertical.json'

if (-not (Get-Command $dotnet -ErrorAction SilentlyContinue)) { throw "Configured .NET executable not found: $dotnet" }
Assert-DORPath -LiteralPath $toolProject -Description 'CookedWeaponMirror project'
Assert-DORPath -LiteralPath $config -Description 'Vertical mirror configuration'
Assert-DORPath -LiteralPath $sourceWeapon -Description 'Audited DRAK source weapon'
Assert-DORPath -LiteralPath $sourceBank -Description 'Audited DRAK source overclock bank'
Assert-DORPath -LiteralPath $sourceItemId -Description 'Audited DRAK source ItemID'
Assert-DORPath -LiteralPath $script:CookedDORContent -Description 'Cooked DOR placeholder content'

$previousDotnetHome = $env:DOTNET_CLI_HOME
$previousNugetPackages = $env:NUGET_PACKAGES
try {
    $env:DOTNET_CLI_HOME = Join-Path $script:DORRoot '.local/dotnet-home'
    $env:NUGET_PACKAGES = Join-Path $script:DORRoot '.local/nuget/packages'
    & $dotnet build --no-restore $toolProject --configuration Release
    if ($LASTEXITCODE -ne 0) {
        throw "CookedWeaponMirror build failed with exit code $LASTEXITCODE"
    }
    & $dotnet $toolDll mirror `
        --config $config `
        --source-weapon $sourceWeapon `
        --source-bank $sourceBank `
        --source-item-id $sourceItemId `
        --cooked-content $cookedContent `
        --result $result
    if ($LASTEXITCODE -ne 0) {
        throw "CookedWeaponMirror failed with exit code $LASTEXITCODE"
    }
}
finally {
    $env:DOTNET_CLI_HOME = $previousDotnetHome
    $env:NUGET_PACKAGES = $previousNugetPackages
}

Assert-DORPath -LiteralPath $result -Description 'Weapon mirror manifest'
Write-Host "DRAK vertical mirror created and verified: $result"
