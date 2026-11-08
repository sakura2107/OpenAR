param([ValidateSet('cpu','gpu')][string]$Profile='cpu', [ValidateSet('Debug','Release')][string]$Configuration='Release', [switch]$Tests, [string]$OnnxRoot='', [string]$MSBuild='')
$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(-not $MSBuild){
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if(Test-Path -LiteralPath $vswhere){$MSBuild=(& $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\Current\Bin\MSBuild.exe' | Select-Object -First 1)}
}
if(-not $MSBuild -or -not (Test-Path -LiteralPath $MSBuild)){throw 'MSBuild not found. Install VS2022 C++ tools or pass -MSBuild.'}
if(-not $OnnxRoot){
    $localProps=Join-Path $taskRoot 'deps.local.props'
    if(Test-Path -LiteralPath $localProps){[xml]$props=Get-Content -LiteralPath $localProps; $key=if($Profile -eq 'gpu'){'OpenAROnnxGpuRoot'}else{'OpenAROnnxCpuRoot'}; $OnnxRoot=[string]$props.Project.PropertyGroup.$key}
}
if(-not (Test-Path -LiteralPath (Join-Path $OnnxRoot 'include\onnxruntime_cxx_api.h'))){throw 'Set ONNX paths in deps.local.props (see deps.example.props), or pass -OnnxRoot.'}
$solution=Join-Path $taskRoot "OpenAR.$Profile.sln"
$buildLog=Join-Path $taskRoot "out\build-$Profile-$Configuration.log"
New-Item -ItemType Directory -Path (Join-Path $taskRoot 'out') -Force | Out-Null
& $MSBuild $solution /m /nr:false /nologo /v:q /clp:ErrorsOnly "/p:Configuration=$Configuration" /p:Platform=x64 "/p:OpenARProfile=$Profile" "/p:ONNXRuntimeRoot=$OnnxRoot" "/flp:logfile=$buildLog;verbosity=normal"
if($LASTEXITCODE -ne 0){throw "Build failed; see $buildLog"}
& (Join-Path $PSScriptRoot 'stage-runtime.ps1') -Profile $Profile -Configuration $Configuration -OnnxRoot $OnnxRoot
if($Tests){
    & (Join-Path $taskRoot "out\$Profile\$Configuration\OpenARTests.exe")
    if($LASTEXITCODE -ne 0){throw 'Regression tests failed.'}
}
