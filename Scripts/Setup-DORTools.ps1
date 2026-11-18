param(
    [string]$DotnetExecutable = 'dotnet',
    [string]$NodeExecutable = 'node',
    [string]$NuGetSource,
    [string]$UE427Root,
    [string]$FSDTemplateRoot
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$privateRoot = Join-Path $projectRoot '.local'
New-Item -ItemType Directory -Path $privateRoot -Force | Out-Null
$previousDotnetHome = $env:DOTNET_CLI_HOME
$previousNugetPackages = $env:NUGET_PACKAGES
try {
    $env:DOTNET_CLI_HOME = Join-Path $privateRoot 'dotnet-home'
    $env:NUGET_PACKAGES = Join-Path $privateRoot 'nuget/packages'
    & $NodeExecutable --version
    if ($LASTEXITCODE -ne 0) { throw 'Node executable check failed' }
    $sdkVersion = & $DotnetExecutable --version
    if ($LASTEXITCODE -ne 0) { throw '.NET SDK executable check failed' }
    Write-Host $sdkVersion
    if ([int]($sdkVersion.Split('.')[0]) -lt 8) { throw "A .NET SDK supporting net8.0 is required; selected SDK is $sdkVersion" }
    foreach ($tool in @('UAssetInspector', 'CookedWeaponMirror')) {
        $project = Join-Path $projectRoot "Analysis/tools/$tool/$tool.csproj"
        $restoreArguments = @('restore', $project, '--packages', $env:NUGET_PACKAGES, '--configfile', (Join-Path $projectRoot 'NuGet.Config'))
        if ($NuGetSource) { $restoreArguments += @('--source', $NuGetSource) }
        & $DotnetExecutable @restoreArguments
        if ($LASTEXITCODE -ne 0) { throw "$tool restore failed" }
        & $DotnetExecutable build $project --configuration Release --no-restore
        if ($LASTEXITCODE -ne 0) { throw "$tool build failed" }
    }
    $configPath = Join-Path $privateRoot 'tool-paths.json'
    $config = if (Test-Path -LiteralPath $configPath) { Get-Content -LiteralPath $configPath -Raw | ConvertFrom-Json -AsHashtable } else { @{} }
    $config.node = $NodeExecutable
    $config.dotnet = $DotnetExecutable
    if ($UE427Root) { $config.ue427Root = [IO.Path]::GetFullPath($UE427Root) }
    if ($FSDTemplateRoot) { $config.fsdTemplate = [IO.Path]::GetFullPath($FSDTemplateRoot) }
    $config | ConvertTo-Json | Set-Content -LiteralPath $configPath -Encoding utf8
    Write-Host "Both tools restored and built; private configuration: $configPath"
}
finally {
    $env:DOTNET_CLI_HOME = $previousDotnetHome
    $env:NUGET_PACKAGES = $previousNugetPackages
}
