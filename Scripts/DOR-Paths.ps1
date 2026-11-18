$ErrorActionPreference = 'Stop'

$script:DORRoot = Split-Path -Parent $PSScriptRoot
$script:WorkspaceRoot = Split-Path -Parent $script:DORRoot
$toolPathsFile = if ($env:DOR_TOOLS_CONFIG) { $env:DOR_TOOLS_CONFIG } else { Join-Path $script:DORRoot '.local/tool-paths.json' }
$toolPaths = if (Test-Path -LiteralPath $toolPathsFile) { Get-Content -LiteralPath $toolPathsFile -Raw -Encoding UTF8 | ConvertFrom-Json } else { $null }
$script:FSDTemplate = if ($toolPaths.fsdTemplate) { [IO.Path]::GetFullPath($toolPaths.fsdTemplate) } else { Join-Path $script:WorkspaceRoot 'FSD-Template' }
$script:UE427 = if ($toolPaths.ue427Root) { [IO.Path]::GetFullPath($toolPaths.ue427Root) } elseif ($env:UE427_ROOT) { $env:UE427_ROOT } else { 'C:\Program Files\Epic Games\UE_4.27' }
$script:NodeExecutable = if ($toolPaths.node) { [string]$toolPaths.node } else { 'node' }
$script:DotnetExecutable = if ($toolPaths.dotnet) { [string]$toolPaths.dotnet } else { 'dotnet' }
$script:UEEditorCmd = Join-Path $script:UE427 'Engine\Binaries\Win64\UE4Editor-Cmd.exe'
$script:UEBuild = Join-Path $script:UE427 'Engine\Build\BatchFiles\Build.bat'
$script:UnrealPak = Join-Path $script:UE427 'Engine\Binaries\Win64\UnrealPak.exe'
$script:Manifest = Join-Path $script:DORRoot 'Mod\Generated\runtime-pairs.json'
$script:RuntimeCsv = Join-Path $script:DORRoot 'Mod\Generated\runtime-pairs.csv'
$script:DORSource = Join-Path $script:DORRoot 'Mod\Source\DOREditor'
$script:DORContent = Join-Path $script:DORRoot 'Mod\Content\DualOverclockReward'
$script:TemplateDORSource = Join-Path $script:FSDTemplate 'Source\DOREditor'
$script:TemplateDORContent = Join-Path $script:FSDTemplate 'Content\DualOverclockReward'
$script:CookedDORContent = Join-Path $script:FSDTemplate 'Saved\Cooked\WindowsNoEditor\FSD\Content\DualOverclockReward'
$script:CookedAssetRegistry = Join-Path $script:FSDTemplate 'Saved\Cooked\WindowsNoEditor\FSD\AssetRegistry.bin'
$script:TemplateManagedContent = Join-Path $script:DORRoot 'Mod\Generated\managed-template-content.json'
$script:LocalManagedContent = Join-Path $script:DORRoot 'Mod\Generated\managed-local-content.json'

function Assert-DORPath {
    param(
        [Parameter(Mandatory = $true)][string]$LiteralPath,
        [Parameter(Mandatory = $true)][string]$Description
    )
    if (-not (Test-Path -LiteralPath $LiteralPath)) {
        throw "$Description not found: $LiteralPath"
    }
}

function Read-DORJsonFile {
    param(
        [Parameter(Mandatory = $true)][string]$LiteralPath,
        [Parameter(Mandatory = $true)][string]$Description
    )
    Assert-DORPath -LiteralPath $LiteralPath -Description $Description
    try {
        return Get-Content -LiteralPath $LiteralPath -Raw -Encoding UTF8 | ConvertFrom-Json
    }
    catch {
        $message = [string]$_.Exception.Message
        if ($message.Length -gt 240) {
            $message = $message.Substring(0, 240) + '...'
        }
        throw "Failed to parse $Description as UTF-8 JSON at '$LiteralPath': $message"
    }
}

function Invoke-WithDORDrives {
    param([Parameter(Mandatory = $true)][scriptblock]$Action)
    $occupied = @([IO.DriveInfo]::GetDrives() | ForEach-Object Name)
    $mappings = @(& subst.exe)
    foreach ($drive in @('R', 'S')) {
        if ($occupied -contains ($drive + ':\') -or ($mappings | Where-Object { $_ -match ('^' + $drive + ':') })) {
            throw "Drive $drive`: is occupied. Free it explicitly before using the legacy build scripts."
        }
    }
    & subst.exe R: $script:FSDTemplate
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to map R: to $script:FSDTemplate"
    }
    & subst.exe S: $script:DORRoot
    if ($LASTEXITCODE -ne 0) {
        & subst.exe R: /D 2>$null
        throw "Failed to map S: to $script:DORRoot"
    }
    try {
        & $Action
    }
    finally {
        & subst.exe S: /D 2>$null
        & subst.exe R: /D 2>$null
    }
}

function Sync-DORSource {
    Assert-DORPath -LiteralPath $script:DORSource -Description 'DOREditor source'
    if (-not (Test-Path -LiteralPath $script:TemplateDORSource)) {
        New-Item -ItemType Junction -Path $script:TemplateDORSource -Target $script:DORSource | Out-Null
        return
    }

    $item = Get-Item -LiteralPath $script:TemplateDORSource -Force
    if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        $targets = @($item.Target)
        if ($targets.Count -ne 1) {
            throw "DOREditor source link has an ambiguous target: $script:TemplateDORSource"
        }
        $target = $targets[0]
        if (-not [IO.Path]::IsPathRooted($target)) {
            $target = Join-Path (Split-Path -Parent $script:TemplateDORSource) $target
        }
        $actual = [IO.Path]::GetFullPath($target).TrimEnd('\')
        $expected = [IO.Path]::GetFullPath($script:DORSource).TrimEnd('\')
        if (-not $actual.Equals($expected, [StringComparison]::OrdinalIgnoreCase)) {
            throw "DOREditor source link targets '$actual', expected '$expected'."
        }
        return
    }

    if (-not $item.PSIsContainer) {
        throw "DOREditor template source is neither a directory nor a junction: $script:TemplateDORSource"
    }
    $expectedFiles = @(Get-DORFileInventory -Root $script:DORSource | ForEach-Object { "$($_.path)|$($_.sha256)" })
    $actualFiles = @(Get-DORFileInventory -Root $script:TemplateDORSource | ForEach-Object { "$($_.path)|$($_.sha256)" })
    $difference = @(Compare-Object -ReferenceObject $expectedFiles -DifferenceObject $actualFiles)
    if ($difference.Count -ne 0) {
        throw "Existing ordinary DOREditor template source is not an exact copy. Refusing to overwrite it: $script:TemplateDORSource"
    }
}

function Get-DORFileInventory {
    param([Parameter(Mandatory = $true)][string]$Root)
    if (-not (Test-Path -LiteralPath $Root)) {
        return @()
    }
    $rootFull = [IO.Path]::GetFullPath($Root).TrimEnd('\')
    return @(Get-ChildItem -LiteralPath $Root -Recurse -File | Sort-Object FullName | ForEach-Object {
        $full = [IO.Path]::GetFullPath($_.FullName)
        [pscustomobject]@{
            path = $full.Substring($rootFull.Length).TrimStart('\').Replace('\', '/')
            sha256 = Get-DORSha256 -LiteralPath $full
        }
    })
}

function Get-DORSha256 {
    param([Parameter(Mandatory = $true)][string]$LiteralPath)
    $stream = [IO.File]::OpenRead($LiteralPath)
    $sha256 = [Security.Cryptography.SHA256]::Create()
    try {
        $hash = $sha256.ComputeHash($stream)
        return ([BitConverter]::ToString($hash)).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $sha256.Dispose()
        $stream.Dispose()
    }
}

function Assert-DORManagedFilePath {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$RelativePath
    )
    if ([IO.Path]::IsPathRooted($RelativePath) -or $RelativePath.Contains('..')) {
        throw "Unsafe managed relative path: $RelativePath"
    }
    $rootFull = [IO.Path]::GetFullPath($Root).TrimEnd('\') + '\'
    $candidate = [IO.Path]::GetFullPath((Join-Path $Root $RelativePath.Replace('/', '\')))
    if (-not $candidate.StartsWith($rootFull, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Managed file escapes its owned root: $candidate"
    }
    return $candidate
}

function Read-DORManagedFiles {
    param([Parameter(Mandatory = $true)][string]$ManifestPath)
    if (-not (Test-Path -LiteralPath $ManifestPath)) {
        return @()
    }
    $record = Read-DORJsonFile -LiteralPath $ManifestPath -Description 'DOR managed-output manifest'
    if ($record.schema -ne 1 -or $null -eq $record.files) {
        throw "Invalid DOR managed-output manifest: $ManifestPath"
    }
    return @($record.files | ForEach-Object { [string]$_.path })
}

function Write-DORManagedInventory {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$ManifestPath
    )
    New-Item -ItemType Directory -Path (Split-Path -Parent $ManifestPath) -Force | Out-Null
    $record = [ordered]@{
        schema = 1
        root = [IO.Path]::GetFullPath($Root)
        generatedAtUtc = [DateTime]::UtcNow.ToString('o')
        files = @(Get-DORFileInventory -Root $Root)
    }
    $record | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $ManifestPath -Encoding utf8
}

function Clear-DORManagedOutput {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$ManifestPath
    )
    New-Item -ItemType Directory -Path $Root -Force | Out-Null
    if (-not (Test-Path -LiteralPath $ManifestPath)) {
        Write-DORManagedInventory -Root $Root -ManifestPath $ManifestPath
    }
    foreach ($relative in @(Read-DORManagedFiles -ManifestPath $ManifestPath)) {
        $candidate = Assert-DORManagedFilePath -Root $Root -RelativePath $relative
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            Remove-Item -LiteralPath $candidate -Force
        }
    }
}

function Sync-DORManagedTree {
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][string]$Destination,
        [Parameter(Mandatory = $true)][string]$ManifestPath
    )
    Assert-DORPath -LiteralPath $Source -Description 'DOR managed source'
    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    $sourceInventory = @(Get-DORFileInventory -Root $Source)
    $sourcePaths = @($sourceInventory | ForEach-Object { $_.path })
    foreach ($relative in @(Read-DORManagedFiles -ManifestPath $ManifestPath)) {
        if ($sourcePaths -notcontains $relative) {
            $stale = Assert-DORManagedFilePath -Root $Destination -RelativePath $relative
            if (Test-Path -LiteralPath $stale -PathType Leaf) {
                Remove-Item -LiteralPath $stale -Force
            }
        }
    }
    foreach ($file in $sourceInventory) {
        $sourceFile = Assert-DORManagedFilePath -Root $Source -RelativePath $file.path
        $destinationFile = Assert-DORManagedFilePath -Root $Destination -RelativePath $file.path
        New-Item -ItemType Directory -Path (Split-Path -Parent $destinationFile) -Force | Out-Null
        Copy-Item -LiteralPath $sourceFile -Destination $destinationFile -Force
    }
    Write-DORManagedInventory -Root $Destination -ManifestPath $ManifestPath
}

function Sync-DORContentToTemplate {
    Sync-DORManagedTree -Source $script:DORContent -Destination $script:TemplateDORContent `
        -ManifestPath $script:TemplateManagedContent
}

function Sync-DORGeneratedContentToMod {
    Sync-DORManagedTree -Source $script:TemplateDORContent -Destination $script:DORContent `
        -ManifestPath $script:LocalManagedContent
}
