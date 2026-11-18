param([string]$EvidenceDirectory)

. (Join-Path $PSScriptRoot 'DOR-Paths.ps1')

$inspector = Join-Path $script:DORRoot 'Analysis\tools\UAssetInspector\bin\Release\net8.0\UAssetInspector.dll'
$evidenceRoot = if ([string]::IsNullOrWhiteSpace($EvidenceDirectory)) {
    Join-Path $script:DORRoot '.local/build/cook-reference'
} else { [IO.Path]::GetFullPath($EvidenceDirectory) }
New-Item -ItemType Directory -Path $evidenceRoot -Force | Out-Null
$jsonRoot = Join-Path $evidenceRoot 'cooked-reference-json'
$inputList = Join-Path $evidenceRoot 'cooked-reference-inputs.txt'
$targets = @(
    @{ Code = 167; Folder = 'Cryospray'; Name = 'DORMerged_31c65a046812632f_01' },
    @{ Code = 328; Folder = 'MicroMissileLauncher'; Name = 'DORMerged_9324d3bdfd67de57_01' }
)
Assert-DORPath -LiteralPath $inspector -Description 'Existing cooked asset inspector'
$inputs = foreach ($target in $targets) {
    $path = Join-Path $script:CookedDORContent "MergedLeaves/$($target.Folder)/$($target.Name).uasset"
    Assert-DORPath -LiteralPath $path -Description "Cooked GF02 reference leaf $($target.Code)"
    [IO.Path]::GetFullPath($path)
}
$inputs | Set-Content -LiteralPath $inputList -Encoding utf8
& $script:DotnetExecutable $inspector $inputList $jsonRoot
if ($LASTEXITCODE -ne 0) { throw 'Cooked GF02 reference inspector failed.' }
$batch = Read-DORJsonFile -LiteralPath (Join-Path $jsonRoot 'batch-result.json') -Description 'Cooked reference inspector result'

function Resolve-DORReference {
    param($Asset, [int]$Index)
    if ($Index -eq 0) { return $null }
    $object = if ($Index -lt 0) { $Asset.Imports[-$Index - 1] } else { $Asset.Exports[$Index - 1] }
    if ($object.OuterIndex -eq 0) { return [string]$object.ObjectName }
    return (Resolve-DORReference $Asset $object.OuterIndex) + '.' + $object.ObjectName
}

$records = foreach ($target in $targets) {
    $input = $inputs[$targets.IndexOf($target)]
    $result = @($batch.results | Where-Object input -eq $input)
    if ($result.Count -ne 1 -or -not $result[0].success) { throw "Cooked reference parse missing: $input" }
    $asset = Read-DORJsonFile -LiteralPath $result[0].output -Description 'Parsed cooked GF02 reference leaf'
    $root = @($asset.Exports | Where-Object ObjectName -eq $target.Name)
    if ($root.Count -ne 1) { throw "Cooked reference root missing: $($target.Name)" }
    $fields = @{}
    foreach ($field in $root[0].Data) { $fields[[string]$field.Name] = $field.Value }
    $record = [ordered]@{ testIndex = $target.Code; parserOutput = $result[0].output; fields = $fields }
    if ($target.Code -eq 167) {
        $damageClass = Resolve-DORReference $asset $fields.DamageClass
        if ($fields.Damage -ne -2 -or $damageClass -ne '/Game/GameElements/Damage/DMG_Cold.DMG_Cold') {
            throw "Cook dropped or changed GF02 167 Damage/DamageClass: $damageClass"
        }
        $record.damageClass = $damageClass
    }
    else {
        $projectileClass = Resolve-DORReference $asset $fields.ProjectileClass
        $category = Resolve-DORReference $asset $fields.Category
        if ($projectileClass -ne '/Game/WeaponsNTools/MicroMissileLauncher/PRJ_MicroMissile.PRJ_MicroMissile_C' -or
            $category -ne '/Game/WeaponsNTools/_UpgradeCategories/UPC_ProjectileSpeed.UPC_ProjectileSpeed' -or
            $fields.amount -ne 1.8000000715255737 -or $fields.IsPercentage -ne $true -or
            $fields.upgradeType -ne 'EProjectileUpgrade::MaxVelocity' -or @($fields.Elements).Count -ne 1) {
            throw "Cook dropped or changed GF02 328 fields: projectile=$projectileClass category=$category"
        }
        $childIndex = [int]$fields.Elements[0].Value
        if ($childIndex -le 0) { throw 'Cooked GF02 328 Elements does not own an export.' }
        $child = $asset.Exports[$childIndex - 1]
        $owner = Resolve-DORReference $asset $child.OuterIndex
        $class = Resolve-DORReference $asset $child.ClassIndex
        if ($child.ObjectName -ne 'ProjectileUpgradeElement_0' -or $owner -ne $target.Name -or
            $class -ne '/Script/FSD.ProjectileUpgradeElement' -or @($child.Data).Count -ne 0) {
            throw 'Cooked GF02 328 child class, ownership or empty payload changed.'
        }
        $record.projectileClass = $projectileClass
        $record.category = $category
        $record.ownedChild = @{ name = $child.ObjectName; owner = $owner; class = $class; data = @($child.Data) }
    }
    $record
}
[ordered]@{
    schema = 'dor.gf02.cooked-reference-readback.v1'
    scope = 'independent serialized cooked properties and import/export references; no gameplay claim'
    passed = $true
    records = @($records)
} | ConvertTo-Json -Depth 30 | Set-Content -LiteralPath (Join-Path $evidenceRoot 'cooked-reference-readback.json') -Encoding utf8
Write-Host 'Cooked GF02 167/328 references and child ownership verified.'
