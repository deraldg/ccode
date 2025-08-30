
<#
  Smoke test for DotTalk++ (ccode)
  Usage: run from repo root →  .\tools\smoke_ccode.ps1 [-Config Release|Debug]
#>

[CmdletBinding()]
param(
  [ValidateSet("Release","Debug")]
  [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

# Discover repo root (tools\ -> repo root)
$Repo     = Split-Path -Parent $MyInvocation.MyCommand.Path | Split-Path -Parent
$BuildDir = Join-Path $Repo "build"
$Target   = "dottalkpp"
$ExePath  = Join-Path $BuildDir "$Config\$Target.exe"

if (-not (Test-Path -LiteralPath $ExePath)) {
  throw "Executable not found: $ExePath`nRun .\tools\rebuild_ccode.ps1 first."
}

# Output folder & logfile
$OutDir = Join-Path $Repo "_smoke"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$Log   = Join-Path $OutDir "smoke-$Config-$stamp.log"

Write-Host ">>> Running smoke test against $ExePath" -ForegroundColor Cyan
Write-Host "Log: $Log"

# Commands to exercise registry, create table, index, seek, and status
$cmds = @'
HELP
CREATE TEMP (ID N(5), NAME C(20))
APPEND_BLANK 3
REPLACE NAME WITH "ALPHA"
INDEX ON NAME TAG N
LIST 5
DESCEND
LIST 3
ASCEND
SEEK "ALPHA"
STATUS
QUIT
'@

# Launch process and feed commands via StdIn
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

# Capture output
$stdout = $proc.StandardOutput.ReadToEnd()
$stderr = $proc.StandardError.ReadToEnd()
$proc.WaitForExit()

$stdout | Set-Content -Path $Log -Encoding UTF8
if ($stderr) {
  "`n--- STDERR ---`n$stderr" | Add-Content -Path $Log -Encoding UTF8
}

# Simple assertions
$fail = $false
if ($stdout -match "Unknown command") { $fail = $true }
if ($stdout -match "failed|error|cannot open|cannot|exception") { $fail = $true }

if ($fail) {
  Write-Host "Smoke test completed with POSSIBLE ISSUES. See log." -ForegroundColor Yellow
  exit 1
} else {
  Write-Host "Smoke test PASSED. See log." -ForegroundColor Green
  exit 0
}
