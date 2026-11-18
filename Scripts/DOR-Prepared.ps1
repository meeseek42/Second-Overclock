# Public preparation is explicit; legacy source paths remain in the legacy branch.
function Assert-DORPreparedInputs {
    param([Parameter(Mandatory)][string]$Receipt)
    & $script:NodeExecutable (Join-Path $PSScriptRoot 'Build-DORAuthoring.mjs') --verify $Receipt | Out-Host
    if($LASTEXITCODE -ne 0){throw 'Prepared input closure changed; rebuild authoring before UE generation/Cook.'}
    return [IO.Path]::GetFullPath((Split-Path -Parent (Split-Path -Parent $Receipt)))
}
function Add-DORPreparedNativeContent {
    param([Parameter(Mandatory)][string]$PreparedRoot,[Parameter(Mandatory)][ValidateSet('generate','cook')][string]$Phase)
    $planPath=Join-Path $script:FSDTemplate ("Saved/DORNativeStage-$Phase.json")
    & $script:NodeExecutable (Join-Path $PSScriptRoot 'Build-DORNativeStagePlan.mjs') (Join-Path $PreparedRoot 'authoring/prepared-inputs.json') $Phase (Join-Path $script:FSDTemplate 'Content') $planPath | Out-Host
    if($LASTEXITCODE -ne 0){throw "Native $Phase staging plan failed"}
    $inventory=Read-DORJsonFile -LiteralPath $planPath -Description 'receipt-backed native phase plan'
    $staged=[System.Collections.Generic.List[string]]::new()
    try {
        foreach($package in $inventory.packages){foreach($member in $package.members){
            $source=Assert-DORManagedFilePath -Root (Join-Path $PreparedRoot 'Content') -RelativePath $member.path
            $target=Assert-DORManagedFilePath -Root (Join-Path $script:FSDTemplate 'Content') -RelativePath $member.path
            if((Get-DORSha256 -LiteralPath $source) -ne $member.sha256){throw "Prepared source changed: $source"}
            if(Test-Path -LiteralPath $target){
                if($member.path.StartsWith('Audio/')){throw "Refusing to replace existing Audio asset: $target"}
                if((Get-DORSha256 -LiteralPath $target) -ne $member.sha256){throw "Conflicting existing native input: $target"}
                continue
            }
            New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
            Copy-Item -LiteralPath $source -Destination $target
            $staged.Add($target)
        }}
        return ,$staged
    }catch{Remove-DORPreparedNativeContent -Files $staged;throw}
}
function Remove-DORPreparedNativeContent {
    param($Files)
    $content=[IO.Path]::GetFullPath((Join-Path $script:FSDTemplate 'Content')).TrimEnd('\')+'\'
    foreach($file in $Files){
        $resolved=[IO.Path]::GetFullPath($file)
        if(-not $resolved.StartsWith($content,[StringComparison]::OrdinalIgnoreCase)){throw "Native cleanup escapes Content: $resolved"}
        if(Test-Path -LiteralPath $resolved -PathType Leaf){Remove-Item -LiteralPath $resolved -Force}
    }
}
