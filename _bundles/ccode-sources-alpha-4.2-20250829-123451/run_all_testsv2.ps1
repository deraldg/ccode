# run_all_tests.ps1  (v2)
# Plan A: just run scripts and capture raw logs.
$ErrorActionPreference = "Stop"

$root     = Split-Path -Parent $MyInvocation.MyCommand.Path
$exe      = Join-Path $root "build\Release\dottalkpp.exe"
$dataDir  = Join-Path $root "data"
$testDir  = Join-Path $root "tests"
$results  = Join-Path $root "results\raw"

if (!(Test-Path $results)) { New-Item -ItemType Directory -Force -Path $results | Out-Null }
if (!(Test-Path $exe)) { throw "Executable not found: $exe" }

Write-Host "DotTalk++ Test Runner (Plan A)"
Write-Host "Executable: $exe"
Write-Host "Data dir  : $dataDir"
Write-Host "Tests dir : $testDir"
Write-Host "Results   : $results"
Write-Host ""

# Only feed .txt scripts directly. For .ps1, we *execute* them (they can drive the exe themselves).
$tests = Get-ChildItem $testDir -File | Where-Object { $_.Extension -in ".txt", ".ps1" }

foreach ($t in $tests) {
    $outLog = Join-Path $results ($t.BaseName + ".log")
    Write-Host ">>> Running $($t.Name) -> $outLog"

    if ($t.Extension -eq ".txt") {
        # Start dottalkpp.exe with redirected stdio and working directory = data
        $psi = New-Object System.Diagnostics.ProcessStartInfo
        $psi.FileName               = $exe
        $psi.WorkingDirectory       = $dataDir
        $psi.UseShellExecute        = $false
        $psi.RedirectStandardInput  = $true
        $psi.RedirectStandardOutput = $true
        $psi.RedirectStandardError  = $true

        $p = New-Object System.Diagnostics.Process
        $p.StartInfo = $psi
        [void]$p.Start()

        # Write each line of the test script to stdin (ensures CRLF)
        Get-Content -Raw $t.FullName -Encoding UTF8 -ReadCount 0 `
            | ForEach-Object {
                # Split to preserve intended line boundaries
                $_ -split "(\r?\n)" | ForEach-Object {
                    if ($_ -notmatch "^\r?\n$") { $p.StandardInput.WriteLine($_) }
                }
            }

        $p.StandardInput.Close()
        $p.WaitForExit()

        $out = $p.StandardOutput.ReadToEnd()
        $err = $p.StandardError.ReadToEnd()
        ($out + $err) | Out-File -FilePath $outLog -Encoding UTF8
    }
    elseif ($t.Extension -eq ".ps1") {
        # Let PowerShell tests run themselves (no '<' redirection!)
        & $t.FullName 2>&1 | Out-File -FilePath $outLog -Encoding UTF8
    }
}

Write-Host "`nAll tests complete. Logs are in $results."

