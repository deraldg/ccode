<#
  Shakedown v1.2: uses comment-aware runner + path/limit wrappers
  Usage:
    .\tools\shakedown_ccode_v1_2.ps1 [-Config Release|Debug]
#>

[CmdletBinding()]
param([ValidateSet("Release","Debug")][string]$Config = "Release")

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$Repo     = Split-Path -Parent $MyInvocation.MyCommand.Path | Split-Path -Parent
$Script   = Join-Path $Repo "tools\_script_shakedown_v1_2.txt"

# Build script file with comments
@"
# Basic create and append variants
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
REPLACE FIRST_NAME WITH ""ALICE""
REPLACE LAST_NAME  WITH ""ZEKE""
REPLACE GPA        WITH 3.14
REPLACE IS_ACTIVE  WITH T
DISPLAY
INDEX ON LAST_NAME TAG L
# Auto-TOP should run now via order_hooks
LIST 3
SEEK LAST_NAME ""ZEKE""
DISPLAY
FIND FIRST_NAME ""ALICE""
DELETE
RECALL
PACK
STATUS
STRUCT
QUIT
"@ | Set-Content -LiteralPath $Script -Encoding UTF8

# Run the script through the comment-aware runner
$runner = Join-Path $Repo "tools\run_script_ccode.ps1"
powershell -ExecutionPolicy Bypass -File $runner -Path $Script -Config $Config | Tee-Object -Variable stdout | Out-Null

# Now test export/import wrappers
$exportOut = Join-Path $Repo "_shakedown\TEMP_out.csv"
powershell -ExecutionPolicy Bypass -File (Join-Path $Repo "tools\export_ccode.ps1") -Table TEMP -Out $exportOut -Config $Config

powershell -ExecutionPolicy Bypass -File (Join-Path $Repo "tools\import_ccode.ps1") -Table T2 -From $exportOut -Limit 3 -Fields '(FIRST_NAME AS Y, ID AS X)' -Config $Config

Write-Host "Shakedown v1.2 completed." -ForegroundColor Green
