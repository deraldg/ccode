$exe = "build/Release/dottalkpp.exe"
$datadir = "data"
$testdir = "tests"
$resultdir = "results/raw"

New-Item -ItemType Directory -Force -Path $resultdir | Out-Null

foreach ($test in Get-ChildItem $testdir -Filter *.txt) {
    $name = [System.IO.Path]::GetFileNameWithoutExtension($test.Name)
    $out  = Join-Path $resultdir "$name.out"
    Write-Host "Running $name ..."
    Get-Content -Raw -Encoding UTF8 $test.FullName | & $exe 2>&1 | Out-File -Encoding UTF8 $out
}
