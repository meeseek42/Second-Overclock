param([Parameter(Mandatory)][string]$GamePak,[Parameter(Mandatory)][string]$ShippingExe,[Parameter(Mandatory)][string]$UnrealPak,[Parameter(Mandatory)][string]$Dotnet,[Parameter(Mandatory)][string]$Inspector,[Parameter(Mandatory)][string]$OutputDirectory,[string]$ReuseReceipt)
. (Join-Path $PSScriptRoot 'DOR-Paths.ps1')
$options=@('--pak',$GamePak,'--shipping-exe',$ShippingExe,'--unreal-pak',$UnrealPak,'--dotnet',$Dotnet,'--inspector',$Inspector,'--output',$OutputDirectory)
if($ReuseReceipt){$options+=@('--reuse-receipt',$ReuseReceipt)}
& $script:NodeExecutable (Join-Path $PSScriptRoot 'Prepare-DORNativeInputs.mjs') @options
if($LASTEXITCODE -ne 0){throw 'Native input preparation failed; no usable completion receipt.'}
