<#
  run_shakedown.ps1  —  Revised to match current DotTalk++ verbs/parsers.
  - Runs inside build\Release\_shakedown\ to avoid touching your real data.
  - Uses APPEND BLANK, SETINDEX, SEEK <field> <value>, FIND <field> <needle>,
    and EXPORT basename (no .csv).
#>

$ErrorActionPreference = "Stop"

# Locate the executable
$exePath = Join-Path $PSScriptRoot "build\Release\dottalkpp.exe"
if (-not (Test-Path $exePath)) {
  $alt = Join-Path $PSScriptRoot "ccode\build\Release\dottalkpp.exe"
  if (Test-Path $alt) { $exePath = $alt }
}
if (-not (Test-Path $exePath)) {
  throw "Could not find dottalkpp.exe. Update `$exePath."
}

# Workspace
$root     = Split-Path -Parent $exePath
$shakeDir = Join-Path $root "_shakedown"
if (Test-Path $shakeDir) { Remove-Item -Recurse -Force $shakeDir }
New-Item -ItemType Directory -Force -Path $shakeDir | Out-Null
Push-Location $shakeDir

# Build the command stream (NO comment lines, only real commands)
$cmds = @(
  "COLOR DEFAULT",
  "HELP",
  "HELP BROKEN",
  "HELP STUBBED",

  # Sanity / Area
  "AREA",
  "SELECT 0",
  "AREA",

  # Create + structure
  "CREATE TEST (ID N(5), NAME C(20), DOB D, ACTIVE L)",
  "STRUCT",
  "FIELDS",
  "STATUS",

  # Append 5 blank records (space form)
  "APPEND BLANK",
  "APPEND BLANK",
  "APPEND BLANK",
  "APPEND BLANK",
  "APPEND BLANK",

  # Populate rows
  "GOTO 1",
  "REPLACE ID WITH 1",
  "REPLACE NAME WITH ""ALICE""",
  "REPLACE ACTIVE WITH .T.",
  "REPLACE DOB WITH {2020-01-15}",

  "GOTO 2",
  "REPLACE ID WITH 2",
  "REPLACE NAME WITH ""BOB""",
  "REPLACE ACTIVE WITH .F.",
  "REPLACE DOB WITH {2019-12-03}",

  "GOTO 3",
  "REPLACE ID WITH 3",
  "REPLACE NAME WITH ""CARL""",
  "REPLACE ACTIVE WITH .T.",
  "REPLACE DOB WITH {2018-07-04}",

  "GOTO 4",
  "REPLACE ID WITH 4",
  "REPLACE NAME WITH ""DORA""",
  "REPLACE ACTIVE WITH .T.",
  "REPLACE DOB WITH {2017-05-21}",

  "GOTO 5",
  "REPLACE ID WITH 5",
  "REPLACE NAME WITH ""ELLA""",
  "REPLACE ACTIVE WITH .F.",
  "REPLACE DOB WITH {2016-10-11}",

  # Listing & counting
  "TOP 3",
  "BOTTOM 2",
  "LIST 20",
  "COUNT",
  "DISPLAY",

  # Delete / undelete / recall / pack lifecycle
  "GOTO 2",
  "DELETE",
  "DISPLAY",
  "UNDELETE",
  "DISPLAY",
  "GOTO 2",
  "DELETE",
  "DISPLAY",
  "RECALL",
  "DISPLAY",
  "PACK",
  "COUNT",
  "LIST 20",

  # Indexing exercise (works even if indexing prints no-op messages)
  "INDEX ON NAME TAG N",
  "SETINDEX N",
  "ASCEND",
  "LIST 20",
  "DESCEND",
  "LIST 20",
  "SEEK NAME CARL",

  # FIND (predicates)
  "FIND NAME DORA",

  # Navigation & status
  "GOTO 3",
  "STATUS",
  "STRUCT",
  "FIELDS",

  # Color & screen
  "COLOR GREEN",
  "LIST 5",
  "COLOR AMBER",
  "LIST 5",
  "COLOR DEFAULT",

  # Multi-area checks
  "SELECT 1",
  "AREA",
  "SELECT 0",
  "AREA",

  # Export/Import — EXPORT auto-appends .csv; pass basename only
  "EXPORT TEST",
  "IMPORT TEST AS TEST2",
  "USE TEST2",
  "LIST 10",

  # Re-open original and final checks
  "USE TEST",
  "LIST 10",

  # --- Extended coverage of remaining verbs ---
  "VERSION",
  "FIELDS",
  "STRUCT",
  "STATUS",
  "DIR",
  "CLEAR",
  "CLS",
  "DUMP",
  "REFRESH",
  "COPY TEST AS TEST_COPY",
  "USE TEST_COPY",
  "LIST 5",
  "USE TEST",
  "GOTO 1",
  "DELETE",
  "UNDELETE",
  "STATUS",

  "QUIT"
)

$cmdFile = Join-Path $shakeDir "shakedown.cmds"
Set-Content -LiteralPath $cmdFile -Value $cmds -Encoding ASCII

Write-Host "Running shakedown from: $shakeDir"
Write-Host "Executable: $exePath"
Write-Host ""

# Pipe to app and tee a log
$logPath = Join-Path $shakeDir "shakedown.log"
(Get-Content -LiteralPath $cmdFile) | & $exePath *>&1 | Tee-Object -FilePath $logPath

Pop-Location

Write-Host ""
Write-Host "Shakedown complete. Log at: $logPath"
