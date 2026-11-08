param(
    [Parameter(Mandatory=$true)][ValidateSet('ja','zh-Hant')][string]$Language,
    [Parameter(Mandatory=$true)][ValidateRange(0,2147483647)][int]$MuMuIndex,
    [Parameter(Mandatory=$true)][string]$Serial,
    [Parameter(Mandatory=$true)][string]$MuMuPath,
    [ValidateRange(100,600000)][int]$IntervalMs=3000,
    [ValidateRange(1,10000)][int]$Count=300,
    [string]$Output=''
)
$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$cli=Join-Path $taskRoot 'out\cpu\Release\OpenAR.exe'
if(-not (Test-Path -LiteralPath $cli -PathType Leaf)){throw 'Build first: ./scripts/build.ps1 -Profile cpu -Tests'}
if(-not (Test-Path -LiteralPath $MuMuPath -PathType Container)){throw 'MuMu installation directory does not exist.'}
if([string]::IsNullOrWhiteSpace($Serial)){throw 'An explicit device serial is required.'}
if(-not $Output){$Output=Join-Path $taskRoot "tests\ocr_corpus\$Language\captures"}
$localConfigRoot=Join-Path $taskRoot 'out\ocr-capture-configs'
New-Item -ItemType Directory -Path $localConfigRoot -Force | Out-Null
$configPath=Join-Path $localConfigRoot (([guid]::NewGuid().ToString('N'))+'.json')
$config=[ordered]@{
    backend='mumu'; device_id=$Serial; serial=$Serial
    mumu_path=[IO.Path]::GetFullPath($MuMuPath); mumu_index=$MuMuIndex
    command_timeout_ms=5000; rotation=0
}
[IO.File]::WriteAllText($configPath,($config | ConvertTo-Json),[Text.UTF8Encoding]::new($false))
Write-Host "Capture only; manually browse the game. Ctrl+C stops. Language=$Language Instance=$MuMuIndex Serial=$Serial"
& $cli capture --device $configPath --language $Language --output $Output --interval-ms $IntervalMs --count $Count
if($LASTEXITCODE -eq 130){Write-Host 'Capture cancelled; saved files retained.'; exit 130}
if($LASTEXITCODE -ne 0){throw "Capture failed (exit $LASTEXITCODE); saved files retained."}
