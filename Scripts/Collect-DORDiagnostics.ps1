[CmdletBinding()]
param(
    [string]$GameRoot,
    [ValidateSet('host', 'client')]
    [string]$Role,
    [string]$Phase = 'current',
    [string]$OutputRoot
)

$ErrorActionPreference = 'Stop'

function Test-DORGameRoot {
    param([string]$Path)
    if ([string]::IsNullOrWhiteSpace($Path)) { return $false }
    return Test-Path -LiteralPath (Join-Path $Path 'FSD') -PathType Container
}

function Get-SafeLabel {
    param([string]$Value, [string]$Fallback)
    if ([string]::IsNullOrWhiteSpace($Value)) { return $Fallback }
    $clean = $Value -replace '[^A-Za-z0-9._-]', '_'
    if ([string]::IsNullOrWhiteSpace($clean)) { return $Fallback }
    return $clean
}

if ([string]::IsNullOrWhiteSpace($GameRoot)) {
    $discovery = @(
        (Get-Location).Path,
        (Split-Path -Parent $PSScriptRoot),
        'D:\SteamLibrary\steamapps\common\Deep Rock Galactic'
    )
    $GameRoot = $discovery | Where-Object { Test-DORGameRoot $_ } | Select-Object -First 1
}

while (-not (Test-DORGameRoot $GameRoot)) {
    $GameRoot = Read-Host 'Deep Rock Galactic game root'
}
$GameRoot = (Resolve-Path -LiteralPath $GameRoot).Path

if ([string]::IsNullOrWhiteSpace($Role)) {
    do { $Role = (Read-Host 'Role (host/client)').Trim().ToLowerInvariant() } while ($Role -notin @('host', 'client'))
}

$phaseLabel = Get-SafeLabel $Phase 'current'
$roleLabel = Get-SafeLabel $Role 'unknown'

if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path (Split-Path -Parent $PSScriptRoot) 'log'
}
$null = New-Item -ItemType Directory -Path $OutputRoot -Force
$OutputRoot = (Resolve-Path -LiteralPath $OutputRoot).Path
$gamePrefix = $GameRoot.TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
if ($OutputRoot.Equals($GameRoot, [StringComparison]::OrdinalIgnoreCase) -or
    $OutputRoot.StartsWith($gamePrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw "OutputRoot must be outside the installed game directory: $OutputRoot"
}

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$runDirectory = Join-Path $OutputRoot ("DOR-diagnostics-{0}-{1}-{2}" -f $stamp, $roleLabel, $phaseLabel)
if (Test-Path -LiteralPath $runDirectory) {
    $runDirectory = "$runDirectory-$([Guid]::NewGuid().ToString('N').Substring(0, 8))"
}
$null = New-Item -ItemType Directory -Path $runDirectory
$runDirectory = (Resolve-Path -LiteralPath $runDirectory).Path

$candidates = @(for ($index = 0; $index -lt 16; $index++) {
    [pscustomobject]@{
        Id = 'normal-{0:d2}' -f $index
        Kind = 'text'
        Priority = $index - 16
        Path = Join-Path $GameRoot ('FSD\Mods\DualOverclockReward\normal-{0:d2}.txt' -f $index)
    }
})
$candidates += @(
    [pscustomobject]@{
        Id = 'alpha79-current'
        Kind = 'text'
        Priority = 0
        Path = Join-Path $GameRoot 'FSD\Mods\DualOverclockReward\diagnostic-current.txt'
    }
)
$candidates += for ($index = 1; $index -le 31; $index++) {
    [pscustomobject]@{
        Id = "alpha79-previous-$index"
        Kind = 'text'
        Priority = $index
        Path = Join-Path $GameRoot "FSD\Mods\DualOverclockReward\diagnostic-previous-$index.txt"
    }
}
$candidates += @(
    [pscustomobject]@{
        Id = 'alpha75-simpleugc-plaintext'
        Kind = 'text'
        Priority = 40
        Path = Join-Path $GameRoot 'FSD\Mods\SaveGames\Mods\DualOverclockReward\diagnostic-current.txt'
    },
    [pscustomobject]@{
        Id = 'requested-saved-plaintext'
        Kind = 'text'
        Priority = 41
        Path = Join-Path $GameRoot 'FSD\Saved\SaveGames\Mods\DualOverclockReward\diagnostic-current.txt'
    },
    [pscustomobject]@{
        Id = 'savegame-fallback'
        Kind = 'binary'
        Priority = 42
        Path = Join-Path $GameRoot 'FSD\Saved\SaveGames\Mods\DOR_Diagnostics_v1.sav'
    }
)

$results = foreach ($candidate in $candidates) {
    $exists = Test-Path -LiteralPath $candidate.Path -PathType Leaf
    $size = $null
    $lastWriteUtc = $null
    $validSnapshot = $false
    $validation = if ($candidate.Kind -eq 'binary') { 'fallback-binary' } else { 'missing' }

    if ($exists) {
        $source = Get-Item -LiteralPath $candidate.Path
        $size = $source.Length
        $lastWriteUtc = $source.LastWriteTimeUtc.ToString('o')
        if ($candidate.Kind -eq 'text') {
            try {
                $content = Get-Content -LiteralPath $source.FullName -Raw
                if ($candidate.Id -like 'normal-*') {
                    $validSnapshot = $source.Length -gt 0 -and $source.Length -le 1048576 -and
                        $content.StartsWith('[DOR-NORMAL]|schema=2|')
                    $validation = if ($validSnapshot) { 'normal-schema-header-ok' } else { 'invalid-normal-header-or-size' }
                } else {
                    $validSnapshot = $source.Length -gt 0 -and
                        $content.Contains('[DOR]|schema=dor-diagnostic-v1|build=')
                    $validation = if ($validSnapshot) { 'schema-header-ok' } else { 'missing-schema-header' }
                }
            }
            catch {
                $validation = "read-failed: $($_.Exception.Message)"
            }
        } else {
            $validSnapshot = $source.Length -gt 0
            $validation = if ($validSnapshot) { 'nonempty-fallback' } else { 'empty-fallback' }
        }
    }

    [pscustomobject]@{
        id = $candidate.Id
        kind = $candidate.Kind
        priority = $candidate.Priority
        path = [IO.Path]::GetFullPath($candidate.Path)
        exists = $exists
        sizeBytes = $size
        lastWriteUtc = $lastWriteUtc
        validSnapshot = $validSnapshot
        validation = $validation
        selected = $false
        copiedFile = $null
        selectedAlias = $null
    }
}

# Prefer the freshest valid plaintext regardless of which historical location
# contains it. The priority only breaks equal timestamps. Source files are never
# modified or deleted by this collector.
$selected = $results |
    Where-Object { $_.kind -eq 'text' -and $_.validSnapshot } |
    Sort-Object @{ Expression = { [DateTime]$_.lastWriteUtc }; Descending = $true }, priority |
    Select-Object -First 1
if ($null -eq $selected) {
    $selected = $results |
        Where-Object { $_.kind -eq 'binary' -and $_.validSnapshot } |
        Sort-Object @{ Expression = { [DateTime]$_.lastWriteUtc }; Descending = $true }, priority |
        Select-Object -First 1
}

foreach ($result in $results) {
    if (-not $result.exists) {
        Write-Host "MISSING $($result.id): $($result.path)"
    } elseif (-not $result.validSnapshot) {
        Write-Host "INVALID $($result.id) [$($result.validation)]: $($result.path)"
    } else {
        $copiedName = if ($result.kind -eq 'text') {
            "$($result.id).txt"
        } else {
            "$($result.id)-DOR_Diagnostics_v1.sav"
        }
        $destination = Join-Path $runDirectory $copiedName
        Copy-Item -LiteralPath $result.path -Destination $destination
        $result.copiedFile = $copiedName
        Write-Host "COPY   $($result.id) -> $copiedName [$($result.lastWriteUtc)]"
        Write-Host "       source: $($result.path)"
        if ($null -ne $selected -and $result.path -eq $selected.path) {
            $selectedName = if ($result.kind -eq 'text') {
                'selected-diagnostic-current.txt'
            } else {
                'selected-DOR_Diagnostics_v1.sav'
            }
            Copy-Item -LiteralPath $result.path -Destination (Join-Path $runDirectory $selectedName)
            $result.selected = $true
            $result.selectedAlias = $selectedName
            Write-Host "SELECT $($result.id) -> $selectedName"
        }
    }
}

$manifest = [ordered]@{
    schemaVersion = 2
    collectedUtc = (Get-Date).ToUniversalTime().ToString('o')
    role = $roleLabel
    phase = $phaseLabel
    gameRoot = $GameRoot
    outputDirectory = $runDirectory
    foundCount = @($results | Where-Object exists).Count
    copiedSourceCount = @($results | Where-Object copiedFile).Count
    selectedSourceId = if ($null -ne $selected) { $selected.id } else { $null }
    selectedSourcePath = if ($null -ne $selected) { $selected.path } else { $null }
    selectedSourceLastWriteUtc = if ($null -ne $selected) { $selected.lastWriteUtc } else { $null }
    selectedAlias = if ($null -ne $selected) { $selected.selectedAlias } else { $null }
    candidates = @($results)
}
$manifestPath = Join-Path $runDirectory 'manifest.json'
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifestPath -Encoding UTF8

Write-Host ''
Write-Host "Found $($manifest.foundCount), copied $($manifest.copiedSourceCount) diagnostic source file(s)."
Write-Host "Output: $runDirectory"
Write-Host "Manifest: $manifestPath"
