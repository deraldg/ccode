
<#
  DotTalk++ Shakedown Script (comprehensive smoke/regression)
  Usage (from repo root):
    .\tools\shakedown_ccode.ps1 [-Config Release|Debug]
#>

[CmdletBinding()]
param(
  [ValidateSet("Release","Debug")]
  [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

# Discover repo root and exe
$Repo     = Split-Path -Parent $MyInvocation.MyCommand.Path | Split-Path -Parent
$BuildDir = Join-Path $Repo "build"
$Target   = "dottalkpp"
$ExePath  = Join-Path $BuildDir "$Config\$Target.exe"

if (-not (Test-Path -LiteralPath $ExePath)) {
  throw "Executable not found: $ExePath`nRun .\tools\rebuild_ccode.ps1 first."
}

# Workspace
$OutDir = Join-Path $Repo "_shakedown"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$stamp  = Get-Date -Format "yyyyMMdd-HHmmss"
$Log    = Join-Path $OutDir "shakedown-$Config-$stamp.log"

Write-Host ">>> Shakedown starting ($Config)" -ForegroundColor Cyan
Write-Host "Log: $Log"

# Compose scripted session
$cmds = @'
HELP
COLOR DEFAULT
CLEAR
CREATE TEMP (ID N(5), FIRST_NAME C(20), LAST_NAME C(20), DOB D, GPA N(4,2), IS_ACTIVE L)
APPEND
APPEND 2
APPEND BLANK
APPEND -B 3
COUNT
BOTTOM 1
RECNO
DISPLAY
GOTO 1
REPLACE FIRST_NAME WITH "ALICE"
REPLACE LAST_NAME  WITH "ZEKE"
REPLACE GPA        WITH 3.14
REPLACE IS_ACTIVE  WITH T
DISPLAY
INDEX ON LAST_NAME TAG L
TOP
LIST 3
SEEK "ZEKE"
DISPLAY
FIND "ALICE"
DELETE
RECALL
PACK
STATUS
STRUCT
# Export/Import round-trip
EXPORT TEMP TO temp_out.csv
USE
CREATE T2 (X N(3), Y C(10))
IMPORT T2 FROM temp_out.csv FIELDS (FIRST_NAME AS Y, ID AS X) LIMIT 3
USE T2
LIST 5
# Error handling
APPEND FROG
APPEND -B X
APPEND -7
# Final state & quit
STATUS
QUIT
'@

# Launch process and pipe commands
$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $ExePath
$psi.RedirectStandardInput  = $true
$psi.RedirectStandardOutput = $true
$psi.RedirectStandardError  = $true
$psi.UseShellExecute = $false
$proc = New-Object System.Diagnostics.Process
$proc.StartInfo = $psi

$null = $proc.Start()
$proc.StandardInput.WriteLine($cmds)
$proc.StandardInput.Close()

$stdout = $proc.StandardOutput.ReadToEnd()
$stderr = $proc.StandardError.ReadToEnd()
$proc.WaitForExit()

$stdout | Set-Content -Path $Log -Encoding UTF8
if ($stderr) {
  "`n--- STDERR ---`n$stderr" | Add-Content -Path $Log -Encoding UTF8
}

# Assertions
$failures = @()

function Assert-Seen($pattern, $desc) {
  if ($stdout -notmatch $pattern) {
    $failures += "Missing: $desc"
  }
}

function Assert-NotSeen($pattern, $desc) {
  if ($stdout -match $pattern) {
    $failures += "Unexpected: $desc"
  }
}

# Core expectations
Assert-Seen "Created TEMP\.dbf"                         "Create TEMP"
Assert-Seen "Appended 1 blank record\."                 "Append 1"
Assert-Seen "Appended 2 blank records\."                "Append 2"
Assert-Seen "Appended 3 blank records\."                "Append -B 3"
Assert-Seen "Replaced FIRST_NAME"                       "Replace FIRST_NAME"
Assert-Seen "Index written:.*\(expr: LAST_NAME"         "Index on LAST_NAME"
Assert-Seen "TOP|Order: ASCENDING|Order: DESCENDING"    "Order navigation hint"
Assert-Seen "Record \d+\s+\r?\n\s+FIRST_NAME\s+= ALICE" "Edited record shows FIRST_NAME"
Assert-Seen "Export"                                    "EXPORT emitted some message"
Assert-Seen "Opened T2\.dbf|USE T2"                     "Switched to T2"
Assert-Seen "Usage: APPEND \[BLANK\|\-B\] \[n\]"        "Bad APPEND usage reported"
Assert-NotSeen "Unknown command"                        "No unknown command errors"
Assert-NotSeen "exception"                              "No raw exception surfaced"
Assert-Seen "STATUS"                                    "Status printed near end"

if ($failures.Count -gt 0) {
  Write-Host "Shakedown completed with issues:" -ForegroundColor Yellow
  $failures | ForEach-Object { Write-Host " - $_" }
  Write-Host "See log: $Log"
  exit 1
} else {
  Write-Host "Shakedown PASSED." -ForegroundColor Green
  Write-Host "Log: $Log"
  exit 0
}
