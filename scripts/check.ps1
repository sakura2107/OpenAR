param([ValidateSet('cpu','gpu')][string]$Profile='cpu')
$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
& (Join-Path $PSScriptRoot 'build.ps1') -Profile $Profile -Tests
$exe=Join-Path $taskRoot "out\$Profile\Release\OpenAR.exe"
$resource=Join-Path $taskRoot 'resources\android-demo'
& $exe make-demo --output $resource
if($LASTEXITCODE -ne 0){throw 'Demo generation failed.'}
& $exe validate --resource $resource
if($LASTEXITCODE -ne 0){throw 'Resource validation failed.'}
foreach($name in @('replay.json','replay-uncertain.json')){
    & $exe replay --resource $resource --replay (Join-Path $resource $name) --trace (Join-Path $taskRoot "out\$Profile\check-traces")
    if($LASTEXITCODE -ne 0){throw "Replay failed: $name"}
}
