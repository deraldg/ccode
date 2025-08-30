# run_all_tests.ps1
# Plan A test runner: execute all .txt or .ps1 scripts in /tests
# Usage: ./run_all_tests.ps1

$ErrorActionPreference = "Stop"

# Paths
$root     = Split-Path -Parent $MyInvocation.MyCommand.Path
$exe      = Join-Path $root "build\Release\dottalkpp.exe"
$dataDir  = Join-Path $root "data"
$testDir  = Join-Path $root "tests"
$results  = Join-Path $root "results\raw"

# Ensure results dir
if (!(Test-Path $results)) { New-Item -ItemType Directory -Force -Path $results | Out-Null }

Write-Host "DotTalk++ Test Runner"
Write-Host "Executable: $exe"
Write-Host "Data dir  : $dataDir"
Write-Host "Tests dir : $testDir"
Write-Host "Results   : $results"
Write-Host ""

# Verify exe exists
if (!(Test-Path $exe)) {
    Write-Error "dottalkpp.exe not found at $exe. Please build first."
    exit 1
}

# Run all test files (txt → redirected input, ps1 → executed)
$tests = Get-ChildItem $testDir -File | Where-Object { $_.Extension -in ".txt", ".ps1" }

foreach ($t in $tests) {
    $outLog = Join-Path $results ($t.BaseName + ".log")
    Write-Host ">>> Running $($t.Name) -> $outLog"

    if ($t.Extension -eq ".txt") {
        # Feed text script to dottalkpp.exe (working dir = data/)
        Push-Location $dataDir
        Get-Content $t.FullName | & $exe | Tee-Object -FilePath $outLog
        Pop-Location
    }
    elseif ($t.Extension -eq ".ps1") {
        # Run the test PowerShell script (assume it drives dottalkpp.exe itself)
        & $t.FullName 2>&1 | Tee-Object -FilePath $outLog
    }
}
Write-Host "`nAll tests complete. Logs are in $results."
