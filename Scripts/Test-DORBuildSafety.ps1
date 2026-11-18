$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'DOR-Paths.ps1')

function Assert-DORTest {
    param(
        [Parameter(Mandatory = $true)][bool]$Condition,
        [Parameter(Mandatory = $true)][string]$Message
    )
    if (-not $Condition) {
        throw $Message
    }
}

$cacheRoot = [IO.Path]::GetFullPath((Join-Path $script:DORRoot 'Analysis\.cache'))
$fixtureRoot = [IO.Path]::GetFullPath((Join-Path $cacheRoot "DORBuildSafetyTest-$PID"))
if (-not $fixtureRoot.StartsWith($cacheRoot.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase) -or
    -not (Split-Path -Leaf $fixtureRoot).StartsWith('DORBuildSafetyTest-')) {
    throw "Unsafe build-safety fixture path: $fixtureRoot"
}

New-Item -ItemType Directory -Path $fixtureRoot -Force | Out-Null
try {
    $validJson = Join-Path $fixtureRoot 'utf8.json'
    '{"message":"\u878d\u5408"}' | Set-Content -LiteralPath $validJson -Encoding utf8
    $parsed = Read-DORJsonFile -LiteralPath $validJson -Description 'UTF-8 fixture'
    Assert-DORTest ($parsed.message.Length -eq 2) 'UTF-8 JSON fixture did not decode correctly.'

    $invalidJson = Join-Path $fixtureRoot 'invalid.json'
    ('{"broken":"' + ('x' * 4096)) | Set-Content -LiteralPath $invalidJson -Encoding utf8
    $parseFailure = ''
    try {
        Read-DORJsonFile -LiteralPath $invalidJson -Description 'invalid fixture' | Out-Null
    }
    catch {
        $parseFailure = [string]$_.Exception.Message
    }
    Assert-DORTest ($parseFailure.StartsWith('Failed to parse invalid fixture as UTF-8 JSON')) `
        'Invalid JSON did not return the bounded parse error.'
    Assert-DORTest ($parseFailure.Length -lt 500) 'Invalid JSON error was not bounded.'

    $managedRoot = Join-Path $fixtureRoot 'managed'
    $managedManifest = Join-Path $fixtureRoot 'managed.json'
    New-Item -ItemType Directory -Path $managedRoot -Force | Out-Null
    'tracked' | Set-Content -LiteralPath (Join-Path $managedRoot 'tracked.txt') -Encoding utf8
    Write-DORManagedInventory -Root $managedRoot -ManifestPath $managedManifest
    'keep' | Set-Content -LiteralPath (Join-Path $managedRoot 'unmanaged.txt') -Encoding utf8
    Clear-DORManagedOutput -Root $managedRoot -ManifestPath $managedManifest
    Assert-DORTest (-not (Test-Path -LiteralPath (Join-Path $managedRoot 'tracked.txt'))) `
        'Managed cleanup did not remove its recorded output.'
    Assert-DORTest (Test-Path -LiteralPath (Join-Path $managedRoot 'unmanaged.txt')) `
        'Managed cleanup removed an unrecorded file.'

    $escapeRejected = $false
    try {
        Assert-DORManagedFilePath -Root $managedRoot -RelativePath '..\outside.txt' | Out-Null
    }
    catch {
        $escapeRejected = $true
    }
    Assert-DORTest $escapeRejected 'Managed cleanup accepted a path outside its owned root.'

    $sourceRoot = Join-Path $fixtureRoot 'source'
    $copyRoot = Join-Path $fixtureRoot 'copy'
    New-Item -ItemType Directory -Path $sourceRoot -Force | Out-Null
    New-Item -ItemType Directory -Path $copyRoot -Force | Out-Null
    'same' | Set-Content -LiteralPath (Join-Path $sourceRoot 'source.txt') -Encoding utf8
    Copy-Item -LiteralPath (Join-Path $sourceRoot 'source.txt') `
        -Destination (Join-Path $copyRoot 'source.txt')
    $savedSource = $script:DORSource
    $savedTemplateSource = $script:TemplateDORSource
    try {
        $script:DORSource = $sourceRoot
        $script:TemplateDORSource = $copyRoot
        Sync-DORSource
        'different' | Set-Content -LiteralPath (Join-Path $copyRoot 'source.txt') -Encoding utf8
        $mismatchRejected = $false
        try {
            Sync-DORSource
        }
        catch {
            $mismatchRejected = $_.Exception.Message -like '*not an exact copy*'
        }
        Assert-DORTest $mismatchRejected 'A stale ordinary source copy was not rejected.'
    }
    finally {
        $script:DORSource = $savedSource
        $script:TemplateDORSource = $savedTemplateSource
    }

    Write-Host 'DOR build-safety fixtures passed: UTF-8 JSON, bounded errors, managed cleanup, path containment, and source-copy mismatch.'
}
finally {
    if (Test-Path -LiteralPath $fixtureRoot) {
        Remove-Item -LiteralPath $fixtureRoot -Recurse -Force
    }
}
