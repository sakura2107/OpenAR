param([ValidateSet('cpu','gpu')][string]$Profile='cpu',[ValidateRange(1,86400)][int]$Seconds=28800,[string]$Resource='',[string]$Device='')
$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$exe=Join-Path $taskRoot "out\$Profile\Release\OpenAR.exe"
$out=Join-Path $taskRoot ('out\soak\'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $out -Force | Out-Null
if(-not $Resource){$Resource=Join-Path $out 'demo'; & $exe make-demo --output $Resource; if($LASTEXITCODE -ne 0){throw 'Demo generation failed.'}}
$deadline=[DateTime]::UtcNow.AddSeconds($Seconds);$count=0
while([DateTime]::UtcNow -lt $deadline){
    $arguments=if($Device){@('run','--resource',$Resource,'--device',$Device,'--trace',(Join-Path $out 'traces'))}else{@('replay','--resource',$Resource,'--replay',(Join-Path $Resource 'replay.json'),'--trace',(Join-Path $out 'traces'))}
    & $exe @arguments >> (Join-Path $out 'runs.log')
    if($LASTEXITCODE -ne 0){throw "Soak failed at run $count; evidence: $out"}
    $count++
    if($count % 100 -eq 0){Write-Host "Completed $count runs; remaining $([int]($deadline-[DateTime]::UtcNow).TotalSeconds)s"}
}
@{runs=$count;duration_seconds=$Seconds;device=$Device;resource=$Resource;completed_utc=[DateTime]::UtcNow.ToString('o')} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $out 'report.json') -Encoding utf8
Write-Host "Soak complete: $count runs; $out"
