param([Parameter(Mandatory)][string]$AssetRoot,[Parameter(Mandatory)][string]$EvidenceDirectory,[Parameter(Mandatory)][string]$BuildTag,[ValidateSet('saved','cooked')][string]$Kind='saved',[switch]$SkipContainment,[switch]$ReuseInspection)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'DOR-Paths.ps1')
$base=[IO.Path]::GetFullPath($AssetRoot)
$scratch=Join-Path ([IO.Path]::GetFullPath($EvidenceDirectory)) ($Kind+'-inspect')
New-Item -ItemType Directory -Path $scratch -Force | Out-Null
$assets=@(Get-ChildItem -LiteralPath $base -Recurse -File -Filter '*.uasset' | Sort-Object FullName | ForEach-Object FullName)
$inputList=Join-Path $scratch 'input.txt'
$assets | Set-Content -LiteralPath $inputList -Encoding utf8
$dllRoot=Join-Path $script:DORRoot 'Analysis/tools/UAssetInspector/bin/Release/net8.0'
if(-not $ReuseInspection){
    & $script:DotnetExecutable (Join-Path $dllRoot 'UAssetInspector.dll') $inputList $scratch
    if($LASTEXITCODE -ne 0){throw "$Kind UAssetInspector failed"}
}
[Reflection.Assembly]::LoadFrom((Join-Path $dllRoot 'Newtonsoft.Json.dll')) | Out-Null
[Reflection.Assembly]::LoadFrom((Join-Path $dllRoot 'UAssetAPI.dll')) | Out-Null

$batch=Get-Content -LiteralPath (Join-Path $scratch 'batch-result.json') -Raw | ConvertFrom-Json
function Get-DORContextSpans($Expression,[string]$ExpressionPath,$Asset){
    if($Expression -isnot [UAssetAPI.Kismet.Bytecode.KismetExpression]){return}
    if($Expression.GetType().Name -in @('EX_Context','EX_Context_FailSilent','EX_ClassContext')){
        $size=$Expression.ContextExpression.GetSize($Asset)
        if($Expression.Offset -ne $size){throw "Invalid serialized context skip at $ExpressionPath"}
        [pscustomobject]@{path=$ExpressionPath;offset=$Expression.Offset;computedSize=$size}
    }
    foreach($field in $Expression.GetType().GetFields()){
        $value=$field.GetValue($Expression)
        if($value -is [UAssetAPI.Kismet.Bytecode.KismetExpression]){Get-DORContextSpans $value ($ExpressionPath+'.'+$field.Name) $Asset}
        elseif($value -is [System.Collections.IEnumerable] -and $value -isnot [string]){
            $index=0
            foreach($child in $value){if($child -is [UAssetAPI.Kismet.Bytecode.KismetExpression]){Get-DORContextSpans $child ($ExpressionPath+'.'+$field.Name+'.'+$index) $Asset};$index++}
        }
    }
}
if($ReuseInspection){
    if(($assets -join "`n") -ne (@($batch.results.input | Sort-Object) -join "`n")){throw 'Reused inspection membership changed.'}
    $prior=Read-DORJsonFile -LiteralPath (Join-Path $scratch 'offsets.json') -Description 'prior inspection bytes'
    foreach($row in $prior){foreach($input in @($row)+@($row.companions)){if((Get-DORSha256 -LiteralPath $input.input) -ne $input.sha256){throw "Reused inspection bytes changed: $($input.input)"}}}
}
$data=@(foreach($item in $batch.results){
    $asset=[UAssetAPI.UAsset]::new($item.input,[UAssetAPI.UnrealTypes.EngineVersion]::VER_UE4_27,$null,[UAssetAPI.CustomSerializationFlags]::None)
    $functions=@(foreach($export in $asset.Exports){
        $name=$export.ObjectName.ToString()
        if(-not $export.ScriptBytecode -or $export.ScriptBytecode.Count -eq 0){continue}
        [uint32]$offset=0
        $rows=@(foreach($expression in $export.ScriptBytecode){
            $size=$expression.GetSize($asset)
            [pscustomobject]@{offset=$offset;size=$size;op=$expression.GetType().Name}
            $offset+=$size
        })
        $contexts=@(for($i=0;$i -lt $export.ScriptBytecode.Count;$i++){Get-DORContextSpans $export.ScriptBytecode[$i] ([string]$i) $asset})
        [pscustomobject]@{name=$name;scriptBytecodeSize=$export.ScriptBytecodeSize;computedSize=$offset;statements=$rows;contexts=$contexts}
    })
    $companions=@(foreach($extension in '.uexp','.ubulk'){
        $path=[IO.Path]::ChangeExtension($item.input,$extension)
        if(Test-Path -LiteralPath $path){
            [pscustomobject]@{input=$path;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
        }
    })
    [pscustomobject]@{input=$item.input;sha256=(Get-FileHash -LiteralPath $item.input -Algorithm SHA256).Hash.ToLowerInvariant();companions=$companions;functions=$functions}
})
$data | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $scratch 'offsets.json') -Encoding utf8
if(-not $SkipContainment){
    & $script:NodeExecutable (Join-Path $PSScriptRoot 'Check-DORContainment.mjs') $Kind $scratch $BuildTag $EvidenceDirectory
    if($LASTEXITCODE -ne 0){throw "$Kind containment semantic readback failed"}
}
