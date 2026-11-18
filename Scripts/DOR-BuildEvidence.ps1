# Maintained evidence helpers. Inventories compare paths/hashes, never timestamps.
function Assert-DORReadbackEvidence {
    param([string]$Directory,[string]$Kind,[string]$BuildTag)
    foreach($name in @("$Kind-containment-validation.json","$Kind-build-readback.json")){
        $report=Read-DORJsonFile -LiteralPath (Join-Path $Directory $name) -Description 'serialized build readback'
        if(-not $report.passed -or $report.build -ne $BuildTag){throw "Missing successful current readback: $name"}
    }
    $containment=Read-DORJsonFile -LiteralPath (Join-Path $Directory "$Kind-containment-validation.json") -Description 'containment readback'
    foreach($file in $containment.inputs){
        if((Get-DORSha256 -LiteralPath $file.input) -ne $file.sha256){throw "Readback input changed: $($file.input)"}
    }
    $semantic=Read-DORJsonFile -LiteralPath (Join-Path $Directory "$Kind-build-readback.json") -Description 'semantic readback'
    foreach($file in $semantic.records){
        if((Get-DORSha256 -LiteralPath $file.semantics) -ne $file.sha256){throw "Semantic evidence changed: $($file.semantics)"}
        foreach($input in $file.inputs){if((Get-DORSha256 -LiteralPath $input.input) -ne $input.sha256){throw "Serialized readback bytes changed: $($input.input)"}}
    }
}
function Write-DORFrozenBuildInputs {
    param([string]$Filename,[string]$PreparedInputs,[string]$BuildTag)
    if(Test-Path -LiteralPath $Filename){throw "Frozen input record already exists: $Filename"}
    $files=@(foreach($relative in @('Mod/Source/DOREditor','Scripts','Authoring','Analysis/src','Analysis/config')){
        Get-ChildItem -LiteralPath (Join-Path $script:DORRoot $relative) -Recurse -File
    })+@(Get-Item -LiteralPath $PreparedInputs,$script:Manifest,$script:RuntimeCsv,(Join-Path $script:DORRoot 'Mod/Generated/display-overrides.json'))
    $inputs=@($files | Sort-Object FullName -Unique | ForEach-Object {[pscustomobject]@{path=$_.FullName;sha256=Get-DORSha256 -LiteralPath $_.FullName}})
    $local=@(Get-DORFileInventory -Root $script:DORContent)
    $template=@(Get-DORFileInventory -Root $script:TemplateDORContent)
    if(($local | ConvertTo-Json -Depth 5 -Compress) -ne ($template | ConvertTo-Json -Depth 5 -Compress)){throw 'Saved local/template inventories differ before Cook.'}
    [ordered]@{schema='dor.frozen-build-inputs.v1';build=$BuildTag;preparedInputs=$PreparedInputs;inputs=$inputs;generated=$local} | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $Filename -Encoding utf8
}
function Assert-DORFrozenBuildInputs {
    param([string]$Filename,[string]$BuildTag)
    $frozen=Read-DORJsonFile -LiteralPath $Filename -Description 'frozen build inputs'
    if($frozen.schema -ne 'dor.frozen-build-inputs.v1' -or $frozen.build -ne $BuildTag){throw 'Wrong frozen build identity.'}
    $null=Assert-DORPreparedInputs -Receipt $frozen.preparedInputs
    foreach($file in $frozen.inputs){if((Get-DORSha256 -LiteralPath $file.path) -ne $file.sha256){throw "Frozen source/runtime input changed: $($file.path)"}}
    $expected=$frozen.generated | ConvertTo-Json -Depth 5 -Compress
    foreach($base in @($script:DORContent,$script:TemplateDORContent)){
        if((@(Get-DORFileInventory -Root $base) | ConvertTo-Json -Depth 5 -Compress) -ne $expected){throw "Frozen generated semantic inventory changed: $base"}
    }
}
