param([Parameter(Mandatory)][string]$TemplateDirectory,[string]$ReceiptPath)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'DOR-Paths.ps1')
$template=[IO.Path]::GetFullPath($TemplateDirectory)
$commit='15fef14b2c20ce25d11b60346f0b7f8db500d7e3'
$tree='7b3fae7864fa2186057ef8790f122553d34dcc84'
function Git-Template([Parameter(ValueFromRemainingArguments)][string[]]$Arguments){
    $result=& git -C $template @Arguments
    if($LASTEXITCODE -ne 0){throw "Template git command failed: $Arguments"}
    return $result
}
if((Git-Template @('rev-parse','HEAD')) -ne $commit -or (Git-Template @('rev-parse','HEAD^{tree}')) -ne $tree){throw 'Template commit/tree differs from the reviewed upstream pin.'}
$tracked=@(Git-Template @('ls-files','Source','Plugins'))
if($tracked.Count -ne 8850 -or @($tracked | Where-Object {$_ -match 'TBBR'}).Count){throw 'Unexpected upstream Source/Plugins inventory.'}
if(@(Git-Template @('ls-files','Content')).Count){throw 'Reviewed upstream has no tracked Content.'}
$dirty=@(Git-Template @('diff','--name-only','HEAD','--'))
$builtOutputs=@($dirty | Where-Object {$_ -match '(^|/)Binaries/Win64/(UE4Editor(?:-[^/]+)?\.(dll|modules)|FSDEditor\.target)$'})
$sourceChanges=@($dirty | Where-Object {$_ -notin @('FSD.uproject','Source/FSDEditor.Target.cs') -and $_ -notin $builtOutputs})
if($sourceChanges.Count){throw "Unreviewed tracked template source/config/third-party changes: $sourceChanges"}
$projectPath=Join-Path $template 'FSD.uproject'
$targetPath=Join-Path $template 'Source/FSDEditor.Target.cs'
$upstreamProject=(Git-Template @('show','HEAD:FSD.uproject')) -join "`n"
$upstreamTarget=((Git-Template @('show','HEAD:Source/FSDEditor.Target.cs')) -join "`n").TrimEnd()
$original=$upstreamProject | ConvertFrom-Json
$project=Read-DORJsonFile -LiteralPath $projectPath -Description 'template project'
if(@($project.Modules | Where-Object Name -eq 'TBBREditor').Count -or (Test-Path -LiteralPath (Join-Path $template 'Source/TBBREditor'))){throw 'Unrelated TBBREditor integration is forbidden.'}
$expected=$upstreamProject | ConvertFrom-Json
$expected.Modules=@($original.Modules)+@([pscustomobject]@{Name='DOREditor';Type='Editor';LoadingPhase='Default'})
$currentText=$project | ConvertTo-Json -Depth 40 -Compress
if($currentText -ne ($original | ConvertTo-Json -Depth 40 -Compress) -and $currentText -ne ($expected | ConvertTo-Json -Depth 40 -Compress)){throw 'Existing project integration differs from the reviewed DOREditor-only integration.'}
$target=[IO.File]::ReadAllText($targetPath).Replace("`r`n","`n").TrimEnd()
$expectedTarget=$upstreamTarget.Replace('"FSDEngine",','"FSDEngine",'+"`n"+'\t\t\t"DOREditor",'.Replace('\t',"`t"))
if($target -ne $upstreamTarget -and $target -ne $expectedTarget){throw 'Existing editor target differs from the reviewed integration.'}
$expected | ConvertTo-Json -Depth 40 | Set-Content -LiteralPath $projectPath -Encoding utf8
[IO.File]::WriteAllText($targetPath,$expectedTarget+"`n")
$script:FSDTemplate=$template
$script:TemplateDORSource=Join-Path $template 'Source/DOREditor'
Sync-DORSource
if(-not $ReceiptPath){$ReceiptPath=Join-Path $script:DORRoot '.local/template-receipt.json'}
New-Item -ItemType Directory -Path (Split-Path -Parent ([IO.Path]::GetFullPath($ReceiptPath))) -Force | Out-Null
[ordered]@{schema='dor.clean-template.v1';upstream='https://github.com/DRG-Modding/FSD-Template';commit=$commit;tree=$tree;sourcePluginEntries=$tracked.Count;trackedContent=0;integration=@('FSD.uproject','Source/FSDEditor.Target.cs','Source/DOREditor');template=$template;compilerOutputOverlay=@($builtOutputs | ForEach-Object {@{path=$_;sha256=Get-DORSha256 -LiteralPath (Join-Path $template $_)}});source=@(Get-DORFileInventory -Root $script:DORSource)} | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $ReceiptPath -Encoding utf8
Write-Host "Verified pinned upstream Source/Plugins and DOREditor-only integration: $ReceiptPath"
