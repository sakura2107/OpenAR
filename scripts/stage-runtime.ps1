param([string]$Profile='cpu',[string]$Configuration='Release',[Parameter(Mandatory=$true)][string]$OnnxRoot)
$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$destination=Join-Path $taskRoot "out\$Profile\$Configuration"
function Copy-RuntimeFile([string]$Source) {
    $target=Join-Path $destination ([IO.Path]::GetFileName($Source))
    # The native capture path may already be using adb.exe. Identical files
    # need no rewrite; never stop a user's ADB server merely to stage binaries.
    if((Test-Path -LiteralPath $target) -and
       (Get-FileHash -LiteralPath $Source -Algorithm SHA256).Hash -eq
       (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash){return}
    Copy-Item -LiteralPath $Source -Destination $target
}
$opencv=if($Configuration -eq 'Debug'){'opencv_world4100d.dll'}else{'opencv_world4100.dll'}
Copy-RuntimeFile (Join-Path $taskRoot "3rdparty\bin\opencv\$opencv")
Copy-RuntimeFile (Join-Path $OnnxRoot 'lib\onnxruntime.dll')
foreach($file in Get-ChildItem -LiteralPath (Join-Path $taskRoot '3rdparty\bin\adb') -File){if($file.Extension -in @('.dll','.exe')){Copy-RuntimeFile $file.FullName}}
if($Profile -eq 'gpu'){
    foreach($name in @('onnxruntime_providers_shared.dll','onnxruntime_providers_cuda.dll')){Copy-RuntimeFile (Join-Path $OnnxRoot "lib\$name")}
    foreach($name in @('cudart64_12.dll','cublas64_12.dll','cublasLt64_12.dll','cufft64_11.dll')){Copy-RuntimeFile (Join-Path $env:CUDA_PATH "bin\$name")}
    [xml]$local=Get-Content -LiteralPath (Join-Path $taskRoot 'deps.local.props')
    $cudnn=[string]$local.Project.PropertyGroup.OpenARCudnnBin
    if(-not $cudnn -or -not (Test-Path -LiteralPath $cudnn)){throw 'Configure OpenARCudnnBin in deps.local.props for GPU runtime staging.'}
    foreach($file in Get-ChildItem -LiteralPath $cudnn -Filter 'cudnn*64_9.dll'){Copy-RuntimeFile $file.FullName}
}
Write-Host "Runtime staged: $destination"
