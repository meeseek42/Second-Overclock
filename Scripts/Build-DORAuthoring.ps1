param([Parameter(Mandatory)][string]$SourceReceipt)
. (Join-Path $PSScriptRoot 'DOR-Paths.ps1')
& $script:NodeExecutable (Join-Path $PSScriptRoot 'Build-DORAuthoring.mjs') --source-receipt $SourceReceipt
if($LASTEXITCODE -ne 0){throw 'Public authoring preparation failed.'}
