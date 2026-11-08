"""Regenerate the MSBuild projects and solutions from source modules (Python 3)."""
from pathlib import Path
from xml.sax.saxutils import escape
import uuid
root=Path(__file__).resolve().parents[1]
projects=root/'projects/vs'; projects.mkdir(parents=True,exist_ok=True)
def sources(folder): return sorted(p.relative_to(root).as_posix() for p in (root/folder).rglob('*.cpp'))
core=[p for p in sources('ar/src') if '/app/' not in p and '/net/' not in p]
ocr=['plugins/ocr/paddle_common/TextDetector.cpp','plugins/ocr/paddle_common/TextRecognizer.cpp','plugins/ocr/paddle_common/Dictionary.cpp']+sources('3rdparty/src/ppocr')
definitions={
 'ARFrameworkLib':('StaticLibrary',core,False),
 'OpenARProxy':('StaticLibrary',sources('ar/src/net'),False),
 'OpenAR':('Application',sources('apps/cli'),False),
 'OpenARTests':('Application',sources('tests'),False),
 'OpenARBenchmark':('Application',sources('benchmarks'),False),
 'OpenARSDKExample':('Application',sources('examples/sdk'),False),
 'MatcherNccCpu':('DynamicLibrary',sources('plugins/matcher/ncc_cpu'),False),
 'MatcherSsimCpu':('DynamicLibrary',sources('plugins/matcher/ssim_cpu'),False),
 'MatcherNccCuda':('DynamicLibrary',sources('plugins/matcher/ncc_cuda')+['plugins/matcher/ncc_cuda/NccPipeline.cu','plugins/common/cuda/CudaImageOps.cu'],True),
 'MatcherSsimCuda':('DynamicLibrary',sources('plugins/matcher/ssim_cuda')+['plugins/matcher/ssim_cuda/SsimPipeline.cu','plugins/common/cuda/CudaImageOps.cu'],True),
 'OcrPaddleCpu':('DynamicLibrary',sources('plugins/ocr/paddle_cpu')+ocr,False),
 'OcrPaddleGpu':('DynamicLibrary',sources('plugins/ocr/paddle_gpu')+ocr,False),
 'ControllerAdb':('DynamicLibrary',sources('plugins/control/adb'),False),
 'ControllerMuMu':('DynamicLibrary',sources('plugins/control/mumu'),False),
 'DeviceMuMu':('DynamicLibrary',sources('plugins/device/mumu'),False),
}
# Each fixture is its own DLL; test implementation sources are not linked into the test executable.
definitions['OpenARTests']=('Application',[p for p in sources('tests') if '/fixtures/' not in p],False)
for p in (root/'tests/fixtures').glob('*.cpp'):
    definitions['Fixture'+p.stem]=('DynamicLibrary',[p.relative_to(root).as_posix()],False)
guids={name:'{'+str(uuid.uuid5(uuid.NAMESPACE_URL,'openar-msbuild/'+name)).upper()+'}' for name in definitions}
for name,(kind,files,cuda) in definitions.items():
    folder=projects/name; folder.mkdir(exist_ok=True)
    guid=guids[name]
    configs=''.join(f'<ProjectConfiguration Include="{c}|x64"><Configuration>{c}</Configuration><Platform>x64</Platform></ProjectConfiguration>' for c in ['Debug','Release'])
    refs=[]
    if kind=='Application':refs=['ARFrameworkLib']
    items=''.join(f'<{"CudaCompile" if p.endswith(".cu") else "ClCompile"} Include="$(OpenARRoot){escape(p.replace("/",chr(92)))}" />' for p in files)
    reference=''.join(f'<ProjectReference Include="..\\{r}\\{r}.vcxproj"><Project>{guids[r]}</Project></ProjectReference>' for r in refs)
    extra=''
    if name.startswith('OcrPaddle'):extra='<ItemDefinitionGroup><Link><AdditionalDependencies>onnxruntime.lib;%(AdditionalDependencies)</AdditionalDependencies><AdditionalLibraryDirectories>$(ONNXRuntimeRoot)\\lib;%(AdditionalLibraryDirectories)</AdditionalLibraryDirectories></Link></ItemDefinitionGroup>'
    if cuda:extra+='''<ItemDefinitionGroup><CudaCompile><CudaRuntime>Shared</CudaRuntime><AdditionalOptions>--std=c++17 %(AdditionalOptions)</AdditionalOptions><Include>$(OpenARRoot)ar\\include;$(OpenARRoot)plugins\\common\\cuda;%(Include)</Include><CompileOut>$(IntDir)%(Filename).cu.obj</CompileOut><KeepDir>$(IntDir)</KeepDir></CudaCompile><Link><AdditionalLibraryDirectories>$(CUDA_PATH)\\lib\\x64;%(AdditionalLibraryDirectories)</AdditionalLibraryDirectories><AdditionalDependencies>cudart.lib;%(AdditionalDependencies)</AdditionalDependencies></Link></ItemDefinitionGroup>'''
    output='fixtures\\' if name.startswith('Fixture') else ('plugins\\' if kind=='DynamicLibrary' else '')
    xml=f'''<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
<ItemGroup Label="ProjectConfigurations">{configs}</ItemGroup>
<PropertyGroup Label="Globals"><ProjectGuid>{guid}</ProjectGuid><Keyword>Win32Proj</Keyword><WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion></PropertyGroup>
<Import Project="$(VCTargetsPath)\\Microsoft.Cpp.Default.props" />
<PropertyGroup Label="Configuration"><ConfigurationType>{kind}</ConfigurationType><PlatformToolset>v143</PlatformToolset><CharacterSet>Unicode</CharacterSet><UseDebugLibraries Condition="'$(Configuration)'=='Debug'">true</UseDebugLibraries></PropertyGroup>
<Import Project="$(VCTargetsPath)\\Microsoft.Cpp.props" />
<Import Project="..\\OpenAR.Common.props" />
{'<Import Project="$(VCTargetsPath)\\BuildCustomizations\\CUDA $(OpenARCudaVersion).props" />' if cuda else ''}
<PropertyGroup><OutDir>$(OpenAROutput){output}</OutDir><IntDir>$(OpenARRoot)out\\obj\\$(OpenARProfile)\\$(Configuration)\\{name}\\</IntDir></PropertyGroup>
{extra}
<ItemGroup>{items}</ItemGroup><ItemGroup>{reference}</ItemGroup>
<Import Project="$(VCTargetsPath)\\Microsoft.Cpp.targets" />
{'<Import Project="$(VCTargetsPath)\\BuildCustomizations\\CUDA $(OpenARCudaVersion).targets" />' if cuda else ''}
</Project>
'''
    (folder/(name+'.vcxproj')).write_text(xml,encoding='utf-8')
for profile in ['cpu','gpu']:
    names=[n for n in definitions if profile=='gpu' or n not in ['MatcherNccCuda','MatcherSsimCuda','OcrPaddleGpu']]
    lines=['Microsoft Visual Studio Solution File, Format Version 12.00','# Visual Studio Version 17']
    for name in names:lines += [f'Project("{{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}}") = "{name}", "projects\\vs\\{name}\\{name}.vcxproj", "{guids[name]}"','EndProject']
    lines+=['Global','\tGlobalSection(SolutionConfigurationPlatforms) = preSolution','\t\tDebug|x64 = Debug|x64','\t\tRelease|x64 = Release|x64','\tEndGlobalSection','\tGlobalSection(ProjectConfigurationPlatforms) = postSolution']
    for name in names:
        for c in ['Debug','Release']:
            lines += [f'\t\t{guids[name]}.{c}|x64.ActiveCfg = {c}|x64',f'\t\t{guids[name]}.{c}|x64.Build.0 = {c}|x64']
    lines+=['\tEndGlobalSection','EndGlobal']
    (root/f'OpenAR.{profile}.sln').write_text('\n'.join(lines)+'\n',encoding='utf-8')
    if profile=='cpu':
        legacy=root/'ARFrameWork.legacy.sln'
        if not legacy.exists() and (root/'ARFrameWork.sln').exists():
            legacy.write_bytes((root/'ARFrameWork.sln').read_bytes())
        (root/'ARFrameWork.sln').write_text('\n'.join(lines)+'\n',encoding='utf-8')
print(f'Generated {len(definitions)} projects and CPU/GPU solutions.')
