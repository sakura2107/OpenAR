# Generates the plugin DLL vcxprojs from a shared template.
# Afterwards run gen_filters.ps1 to (re)build the directory-mirroring .filters.
$ErrorActionPreference = 'Stop'
Set-Location 'C:\Users\cjp\Desktop\OpenAR-main\build'

$template = @'
<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup Label="ProjectConfigurations">
    <ProjectConfiguration Include="Debug|x64">
      <Configuration>Debug</Configuration>
      <Platform>x64</Platform>
    </ProjectConfiguration>
    <ProjectConfiguration Include="Release|x64">
      <Configuration>Release</Configuration>
      <Platform>x64</Platform>
    </ProjectConfiguration>
  </ItemGroup>
  <PropertyGroup Label="Globals">
    <VCProjectVersion>17.0</VCProjectVersion>
    <Keyword>Win32Proj</Keyword>
    <ProjectGuid>__GUID__</ProjectGuid>
    <RootNamespace>__NAME__</RootNamespace>
    <WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion>
  </PropertyGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.Default.props" />
  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Debug|x64'" Label="Configuration">
    <ConfigurationType>DynamicLibrary</ConfigurationType>
    <UseDebugLibraries>true</UseDebugLibraries>
    <PlatformToolset>v143</PlatformToolset>
    <CharacterSet>Unicode</CharacterSet>
  </PropertyGroup>
  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Release|x64'" Label="Configuration">
    <ConfigurationType>DynamicLibrary</ConfigurationType>
    <UseDebugLibraries>false</UseDebugLibraries>
    <PlatformToolset>v143</PlatformToolset>
    <WholeProgramOptimization>true</WholeProgramOptimization>
    <CharacterSet>Unicode</CharacterSet>
  </PropertyGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.props" />
  <Import Project="$(SolutionDir)build\Plugins.Common.props" />
__CUDA_IMPORT__
  <ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Debug|x64'">
    <Link>
      <SubSystem>Windows</SubSystem>
      <AdditionalLibraryDirectories>__LIBDIRS__</AdditionalLibraryDirectories>
      <AdditionalDependencies>__LIBS_DEBUG__</AdditionalDependencies>
    </Link>
  </ItemDefinitionGroup>
  <ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Release|x64'">
    <Link>
      <SubSystem>Windows</SubSystem>
      <AdditionalLibraryDirectories>__LIBDIRS__</AdditionalLibraryDirectories>
      <AdditionalDependencies>__LIBS_RELEASE__</AdditionalDependencies>
    </Link>
  </ItemDefinitionGroup>
  <ItemGroup>
__HEADERS__
  </ItemGroup>
  <ItemGroup>
__SOURCES__
  </ItemGroup>
__CUDA_ITEMS__
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" />
__CUDA_TARGETS__
</Project>
'@

$opencvDir = '$(SolutionDir)3rdparty\lib\opencv;'
$projects = @(
    @{ name='MatcherSsimCpu'; guid='{1D9A6C32-5E74-4B8F-9A21-3F6C8D90E2A4}'; cuda=$false;
       libsD='opencv_world4100d.lib'; libsR='opencv_world4100.lib';
       headers=@('ar\include\plugin\PluginApi.h','ar\include\vision\Matcher.h','plugins\matcher\ssim_cpu\SsimCpu.h');
       sources=@('plugins\matcher\ssim_cpu\SsimCpu.cpp','plugins\matcher\ssim_cpu\PluginExports.cpp') },
    @{ name='OcrPaddleCpu'; guid='{2E8B7D43-6F85-4C9A-B132-4A7D9E01F3B5}'; cuda=$false;
       libsD='opencv_world4100d.lib;onnxruntime.lib'; libsR='opencv_world4100.lib;onnxruntime.lib';
       headers=@('ar\include\plugin\PluginApi.h','ar\include\vision\Ocr.h','plugins\ocr\paddle_common\PaddleEngine.h','plugins\ocr\paddle_common\TextDetector.h','plugins\ocr\paddle_common\TextRecognizer.h','plugins\ocr\paddle_common\Dictionary.h','plugins\ocr\paddle_common\OrtSession.hpp','3rdparty\include\ppocr\clipper.h','3rdparty\include\ppocr\utility.h');
       sources=@('plugins\ocr\paddle_cpu\PluginExports.cpp','plugins\ocr\paddle_common\TextDetector.cpp','plugins\ocr\paddle_common\TextRecognizer.cpp','plugins\ocr\paddle_common\Dictionary.cpp','3rdparty\src\ppocr\clipper.cpp','3rdparty\src\ppocr\postprocess_op.cpp','3rdparty\src\ppocr\utility.cpp') },
    @{ name='OcrPaddleGpu'; guid='{3F9C8E54-7A96-4DAB-8243-5B8EAF12A4C6}'; cuda=$false;
       libsD='opencv_world4100d.lib;onnxruntime.lib'; libsR='opencv_world4100.lib;onnxruntime.lib';
       headers=@('ar\include\plugin\PluginApi.h','ar\include\vision\Ocr.h','plugins\ocr\paddle_common\PaddleEngine.h','plugins\ocr\paddle_common\TextDetector.h','plugins\ocr\paddle_common\TextRecognizer.h','plugins\ocr\paddle_common\Dictionary.h','plugins\ocr\paddle_common\OrtSession.hpp','3rdparty\include\ppocr\clipper.h','3rdparty\include\ppocr\utility.h');
       sources=@('plugins\ocr\paddle_gpu\PluginExports.cpp','plugins\ocr\paddle_common\TextDetector.cpp','plugins\ocr\paddle_common\TextRecognizer.cpp','plugins\ocr\paddle_common\Dictionary.cpp','3rdparty\src\ppocr\clipper.cpp','3rdparty\src\ppocr\postprocess_op.cpp','3rdparty\src\ppocr\utility.cpp') },
    @{ name='ControllerAdb'; guid='{4A8D9F65-8BA7-4E9C-9354-6C9FBA23B5D7}'; cuda=$false;
       libsD='opencv_world4100d.lib'; libsR='opencv_world4100.lib';
       headers=@('ar\include\plugin\PluginApi.h','ar\include\control\Controller.h','ar\include\control\AdbClient.hpp','plugins\control\adb\AdbController.h');
       sources=@('plugins\control\adb\PluginExports.cpp','plugins\control\adb\AdbController.cpp') },
    @{ name='ControllerMuMu'; guid='{5B9EAF76-9CB8-4FAD-8465-7DAFCB34C6E8}'; cuda=$false;
       libsD='opencv_world4100d.lib'; libsR='opencv_world4100.lib';
       headers=@('ar\include\plugin\PluginApi.h','ar\include\control\Controller.h','ar\include\control\AdbClient.hpp','plugins\control\mumu\MuMuController.h');
       sources=@('plugins\control\mumu\PluginExports.cpp','plugins\control\mumu\MuMuController.cpp') },
    @{ name='DeviceMuMu'; guid='{6CAFBA87-ADC9-40BE-9576-8EBFDC45D7F9}'; cuda=$false;
       libsD=''; libsR='';
       headers=@('ar\include\plugin\PluginApi.h','ar\include\device\DeviceController.h','plugins\device\mumu\MuMuDeviceController.h');
       sources=@('plugins\device\mumu\PluginExports.cpp','plugins\device\mumu\MuMuDeviceController.cpp') }
)

function RootRef([string]$projectRelative) {
    # build\<Project>\vcxproj -> repo root is ..\..
    return '..\..\' + $projectRelative
}

foreach ($p in $projects) {
    $xml = $template

    if ($p.cuda) {
        $xml = $xml.Replace('__CUDA_IMPORT__', '  <ImportGroup Label="ExtensionSettings">' + "`r`n" +
            '    <Import Project="$(VCTargetsPath)\BuildCustomizations\CUDA 12.9.props" Condition="Exists(''$(VCTargetsPath)\BuildCustomizations\CUDA 12.9.props'')" />' + "`r`n" +
            '  </ImportGroup>')
        $xml = $xml.Replace('__CUDA_ITEMS__', '')
        $xml = $xml.Replace('__CUDA_TARGETS__', '')
    } else {
        $xml = $xml.Replace('__CUDA_IMPORT__', '')
        $xml = $xml.Replace('__CUDA_ITEMS__', '')
        $xml = $xml.Replace('__CUDA_TARGETS__', '')
    }

    $libdirs = ''
    if ($p.libsD -match 'opencv') { $libdirs += $opencvDir }
    if ($p.libsD -match 'onnxruntime') { $libdirs += '$(ONNXRuntimeRoot)\lib;' }
    $xml = $xml.Replace('__LIBDIRS__', $libdirs + '%(AdditionalLibraryDirectories)')
    $xml = $xml.Replace('__LIBS_DEBUG__', $p.libsD + ';%(AdditionalDependencies)')
    $xml = $xml.Replace('__LIBS_RELEASE__', $p.libsR + ';%(AdditionalDependencies)')

    $headers = ($p.headers | ForEach-Object { '    <ClInclude Include="' + (RootRef $_) + '" />' }) -join "`r`n"
    $sources = ($p.sources | ForEach-Object { '    <ClCompile Include="' + (RootRef $_) + '" />' }) -join "`r`n"
    $xml = $xml.Replace('__HEADERS__', $headers)
    $xml = $xml.Replace('__SOURCES__', $sources)

    $xml = $xml.Replace('__GUID__', $p.guid)
    $xml = $xml.Replace('__NAME__', $p.name)

    $dir = Join-Path 'C:\Users\cjp\Desktop\OpenAR-main\build' $p.name
    if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir | Out-Null }
    [IO.File]::WriteAllText((Join-Path $dir ($p.name + '.vcxproj')), $xml)

    # Generate the filters file from the source lists (plugin vs framework).
    $fx = @'
<?xml version="1.0" encoding="utf-8"?>
<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup>
    <Filter Include="plugin">
      <UniqueIdentifier>{__UF__}</UniqueIdentifier>
    </Filter>
    <Filter Include="framework">
      <UniqueIdentifier>{__UF2__}</UniqueIdentifier>
    </Filter>
  </ItemGroup>
__ENTRIES__
</Project>
'@
    $entries = ''
    foreach ($h in $p.headers) {
        $filter = if ($h -like 'plugins\*') { 'plugin' } else { 'framework' }
        $entries += '  <ClInclude Include="' + (RootRef $h) + '">' + "`r`n" +
                    '    <Filter>' + $filter + '</Filter>' + "`r`n" + '  </ClInclude>' + "`r`n"
    }
    foreach ($s in $p.sources) {
        $filter = if ($s -like 'plugins\*') { 'plugin' } else { 'framework' }
        $entries += '  <ClCompile Include="' + (RootRef $s) + '">' + "`r`n" +
                    '    <Filter>' + $filter + '</Filter>' + "`r`n" + '  </ClCompile>' + "`r`n"
    }
    $fx = $fx.Replace('__ENTRIES__', $entries.TrimEnd())
    $fx = $fx.Replace('__UF__', [guid]::NewGuid().ToString())
    $fx = $fx.Replace('__UF2__', [guid]::NewGuid().ToString())
    [IO.File]::WriteAllText((Join-Path $dir ($p.name + '.vcxproj.filters')), $fx)
}
