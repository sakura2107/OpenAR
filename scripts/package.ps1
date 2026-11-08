param([ValidateSet('cpu','gpu')][string]$Profile='cpu',[switch]$IncludeModels,[switch]$IncludeProxy)
# Requires PowerShell 7 for portable-path hashing; Windows PowerShell can build.
$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$source=Join-Path $taskRoot "out\$Profile\Release"
$destination=Join-Path $taskRoot "dist\OpenAR-$Profile"
if(-not (Test-Path -LiteralPath (Join-Path $source 'OpenAR.exe'))){throw 'Build Release before packaging.'}
$packageRoot=[IO.Path]::GetFullPath((Join-Path $taskRoot 'dist'))+[IO.Path]::DirectorySeparatorChar
$resolvedDestination=[IO.Path]::GetFullPath($destination)
if(-not $resolvedDestination.StartsWith($packageRoot,[StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetFileName($resolvedDestination) -ne "OpenAR-$Profile"){throw 'Unexpected package directory.'}
if(Test-Path -LiteralPath $resolvedDestination){Remove-Item -LiteralPath $resolvedDestination -Recurse -Force}
New-Item -ItemType Directory -Path $destination,(Join-Path $destination 'plugins'),(Join-Path $destination 'sdk\include'),(Join-Path $destination 'sdk\lib') -Force | Out-Null
foreach($file in Get-ChildItem -LiteralPath $source -File){if($file.Extension -eq '.dll' -or $file.Name -in @('OpenAR.exe','adb.exe','OpenARBenchmark.exe','OpenARSDKExample.exe')){Copy-Item -LiteralPath $file.FullName -Destination $destination}}
foreach($file in Get-ChildItem -LiteralPath (Join-Path $source 'plugins') -Filter '*.dll'){Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $destination 'plugins')}
Copy-Item -LiteralPath (Join-Path $taskRoot 'ar\include') -Destination (Join-Path $destination 'sdk') -Recurse -Force
Copy-Item -LiteralPath (Join-Path $taskRoot 'include\ar') -Destination (Join-Path $destination 'sdk\include') -Recurse -Force
Copy-Item -LiteralPath (Join-Path $taskRoot 'examples\sdk') -Destination (Join-Path $destination 'sdk\examples') -Recurse -Force
Copy-Item -LiteralPath (Join-Path $taskRoot '3rdparty\include\opencv2') -Destination (Join-Path $destination 'sdk\include') -Recurse -Force
Copy-Item -LiteralPath (Join-Path $taskRoot '3rdparty\include\rapidjson') -Destination (Join-Path $destination 'sdk\include') -Recurse -Force
Copy-Item -LiteralPath (Join-Path $source 'ARFrameworkLib.lib') -Destination (Join-Path $destination 'sdk\lib')
Copy-Item -LiteralPath (Join-Path $taskRoot '3rdparty\lib\opencv\opencv_world4100.lib') -Destination (Join-Path $destination 'sdk\lib')
if($IncludeProxy){Copy-Item -LiteralPath (Join-Path $source 'OpenARProxy.lib') -Destination (Join-Path $destination 'sdk\lib')}
if($IncludeModels){New-Item -ItemType Directory -Path (Join-Path $destination 'models') -Force | Out-Null;foreach($name in @('det.sim.onnx','rec.sim.onnx','ppocr_keys_v1.txt')){Copy-Item -LiteralPath (Join-Path $taskRoot "models\$name") -Destination (Join-Path $destination 'models')}}
Copy-Item -LiteralPath (Join-Path $taskRoot 'examples\device.adb.json') -Destination (Join-Path $destination 'device.example.json')
Copy-Item -LiteralPath (Join-Path $taskRoot 'README.md') -Destination $destination
foreach($name in @('LICENSE','LICENSE.txt','LICENSE.md')){if(Test-Path -LiteralPath (Join-Path $taskRoot $name)){Copy-Item -LiteralPath (Join-Path $taskRoot $name) -Destination $destination}}
Copy-Item -LiteralPath (Join-Path $taskRoot 'docs') -Destination $destination -Recurse -Force
New-Item -ItemType Directory -Path (Join-Path $destination 'data\samples') -Force | Out-Null
foreach($sampleName in @('1.png','2.png','3.jpg')){Copy-Item -LiteralPath (Join-Path $taskRoot "data\samples\$sampleName") -Destination (Join-Path $destination 'data\samples')}
$localProps=Join-Path $taskRoot 'deps.local.props'
if(Test-Path -LiteralPath $localProps){
    [xml]$dependencyProps=Get-Content -LiteralPath $localProps
    $dependencyKey=if($Profile -eq 'gpu'){'OpenAROnnxGpuRoot'}else{'OpenAROnnxCpuRoot'}
    $onnxRoot=[string]$dependencyProps.Project.PropertyGroup.$dependencyKey
    $notices=Join-Path $destination 'licenses\onnxruntime'
    New-Item -ItemType Directory -Path $notices -Force | Out-Null
    foreach($noticeName in @('LICENSE','ThirdPartyNotices.txt','VERSION_NUMBER')){
        $noticePath=Join-Path $onnxRoot $noticeName
        if(Test-Path -LiteralPath $noticePath){Copy-Item -LiteralPath $noticePath -Destination $notices}
    }
}
& (Join-Path $destination 'OpenAR.exe') make-demo --output (Join-Path $destination 'resources\android-demo')
if($LASTEXITCODE -ne 0){throw 'Packaged CLI demo failed.'}
& (Join-Path $destination 'OpenAR.exe') schemas --output (Join-Path $destination 'schemas')
if($LASTEXITCODE -ne 0){throw 'Schema export failed.'}
$files=Get-ChildItem -LiteralPath $destination -Recurse -File | Where-Object {$_.Name -ne 'SHA256SUMS.txt'} | Sort-Object FullName
$lines=foreach($file in $files){$relative=[IO.Path]::GetRelativePath($destination,$file.FullName);$hash=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant();"$hash  $relative"}
[IO.File]::WriteAllLines((Join-Path $destination 'SHA256SUMS.txt'),$lines,[Text.UTF8Encoding]::new($false))
Write-Host "Package ready: $destination (MSVC x64 runtime required)"
