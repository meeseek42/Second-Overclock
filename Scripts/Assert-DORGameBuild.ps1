$ErrorActionPreference = 'Stop'

$dorRoot = Split-Path -Parent $PSScriptRoot
$pathsFile = Join-Path $dorRoot 'Analysis\config\game-paths.local.json'
$buildFile = Join-Path $dorRoot 'Analysis\config\game-build.json'

if (-not (Test-Path -LiteralPath $pathsFile)) {
    throw "Local game paths config not found: $pathsFile"
}
if (-not (Test-Path -LiteralPath $buildFile)) {
    throw "Pinned game build config not found: $buildFile"
}

$paths = Get-Content -LiteralPath $pathsFile -Raw -Encoding UTF8 | ConvertFrom-Json
$build = Get-Content -LiteralPath $buildFile -Raw -Encoding UTF8 | ConvertFrom-Json
$gamePak = [IO.Path]::GetFullPath([string]$paths.gamePak)
if (-not (Test-Path -LiteralPath $gamePak)) {
    throw "Game Pak not found: $gamePak"
}

$expected = ([string]$build.pakSha256).ToUpperInvariant()
$stream = [IO.File]::OpenRead($gamePak)
try {
    $sha256 = [Security.Cryptography.SHA256]::Create()
    try {
        $actual = ([BitConverter]::ToString($sha256.ComputeHash($stream))).Replace('-', '')
    }
    finally {
        $sha256.Dispose()
    }
}
finally {
    $stream.Dispose()
}
if ([string]::IsNullOrWhiteSpace($expected) -or $actual -ne $expected) {
    Write-Warning @"
DOR-W-GAME-BUILD-MISMATCH: continuing because the game-build gate is disabled.
Pinned build: $($build.build)
Expected Pak SHA-256: $expected
Actual Pak SHA-256:   $actual
This package is allowed for targeted testing, but the 160 overclock contracts
have not been comprehensively revalidated against this game build.
"@
}
else {
    Write-Host "DOR game build verified: $($build.build) ($actual)"
}
