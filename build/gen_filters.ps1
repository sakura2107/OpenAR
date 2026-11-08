# Regenerates the .filters files of every plugin DLL project so that the
# filter tree mirrors the directory tree (filter name = the file's directory
# relative to the repository root). Run after adding files to a plugin.
$ErrorActionPreference = 'Stop'
Set-Location 'C:\Users\cjp\Desktop\OpenAR-main\build'

$pattern = '(ClInclude|ClCompile|CudaCompile)\s+Include="([^"]+)"'
$projects = 'MatcherSsimCuda','MatcherSsimCpu','OcrPaddleCpu','OcrPaddleGpu',
            'ControllerAdb','ControllerMuMu','DeviceMuMu'

foreach ($name in $projects) {
    $vcxPath = Join-Path $name ($name + '.vcxproj')
    $vcx = [IO.File]::ReadAllText((Join-Path (Get-Location) $vcxPath))
    $items = [regex]::Matches($vcx, $pattern) | ForEach-Object { $_.Groups[2].Value }

    # filter (directory relative to repo root) -> items
    $byFilter = @{}
    foreach ($i in $items) {
        $rel = $i -replace '^(\.\.\\)+', ''           # strip the project-relative climb
        $filter = [IO.Path]::GetDirectoryName($rel)   # e.g. plugins\matcher\ssim_cuda
        if (-not $byFilter.ContainsKey($filter)) { $byFilter[$filter] = @() }
        $byFilter[$filter] += $i
    }

    $guids = @{}
    foreach ($f in $byFilter.Keys) { $guids[$f] = [guid]::NewGuid().ToString().ToUpper() }

    $groups = '  <ItemGroup>'
    foreach ($f in $byFilter.Keys) {
        $groups += "`r`n" +
            '    <Filter Include="' + $f + '">' + "`r`n" +
            '      <UniqueIdentifier>{' + $guids[$f] + '}</UniqueIdentifier>' + "`r`n" +
            '    </Filter>'
    }
    $groups += "`r`n" + '  </ItemGroup>'

    $entries = '  <ItemGroup>'
    foreach ($f in $byFilter.Keys) {
        foreach ($i in $byFilter[$f]) {
            $kind = if ($i -like '*.cu') { 'CudaCompile' }
                    elseif ($i -match '\.(h|hpp)$') { 'ClInclude' }
                    else { 'ClCompile' }
            $entries += "`r`n" +
                '    <' + $kind + ' Include="' + $i + '">' + "`r`n" +
                '      <Filter>' + $f + '</Filter>' + "`r`n" +
                '    </' + $kind + '>'
        }
    }
    $entries += "`r`n" + '  </ItemGroup>'

    $xml = '<?xml version="1.0" encoding="utf-8"?>' + "`r`n" +
        '<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">' + "`r`n" +
        $groups + "`r`n" + $entries + "`r`n" +
        '</Project>'
    [IO.File]::WriteAllText((Join-Path (Get-Location) ($name + '\' + $name + '.vcxproj.filters')), $xml)
    Write-Host ('filters regenerated: ' + $name)
}
