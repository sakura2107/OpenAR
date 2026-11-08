$ErrorActionPreference = 'Stop'
Set-Location 'C:\Users\cjp\Desktop\OpenAR-main\build'

$pattern = '(ClInclude|ClCompile|CudaCompile)\s+Include="([^"]+)"'
Get-ChildItem -Recurse -Include *.vcxproj | ForEach-Object {
    $proj = $_
    $dir = $proj.DirectoryName
    $vcx = [IO.File]::ReadAllText($proj.FullName)
    $items = [regex]::Matches($vcx, $pattern) | ForEach-Object { $_.Groups[2].Value }
    $fltPath = Join-Path $dir ($proj.BaseName + '.vcxproj.filters')
    $flt = if (Test-Path $fltPath) { [IO.File]::ReadAllText($fltPath) } else { '' }
    $missingOnDisk = @()
    $missingInFilters = @()
    foreach ($i in $items) {
        $full = [IO.Path]::GetFullPath((Join-Path $dir $i))
        if (-not (Test-Path $full)) { $missingOnDisk += $i }
        if ($flt -notmatch [regex]::Escape($i)) { $missingInFilters += $i }
    }
    Write-Host ("== " + $proj.BaseName + "  items=" + $items.Count)
    if ($missingOnDisk.Count) { Write-Host ("  ON-DISK-MISSING: " + ($missingOnDisk -join ', ')) }
    if ($missingInFilters.Count) { Write-Host ("  FILTER-MISSING: " + ($missingInFilters -join ', ')) }
}
