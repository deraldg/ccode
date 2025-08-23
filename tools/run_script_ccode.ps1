<#
  Comment-aware script runner for DotTalk++
  Filters out blank lines and lines starting with '#' before piping to CLI.
  Usage:
    .\tools\run_script_ccode.ps1 -Path .\myscript.txt [-Config Release|Debug]
#>

[CmdletBinding()]
param(
  [Parameter(Mandatory=$true)]
  [string]$Path,
  [ValidateSet("Release","Debug")]
  [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

# Discover repo and exe
$Repo     = Split-Path -Parent $MyInvocation.MyCommand.Path | Split-Path -Parent
$BuildDir = Join-Path $Repo "build"
$ExePath  = Join-Path $BuildDir "$Config\dottalkpp.exe"

if (-not (Test-Path -LiteralPath $ExePath)) {
  throw "Executable not found: $ExePath`nRun .\tools\rebuild_ccode.ps1 first."
}
if (-not (Test-Path -LiteralPath $Path)) {
  throw "Script file not found: $Path"
}

# Pre-filter script
$lines = Get-Content -LiteralPath $Path
$filtered = $lines | Where-Object {
  $t = $_.Trim()
  return ($t.Length -gt 0) -and (-not $t.StartsWith("#"))
}

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
$filtered | ForEach-Object { $proc.StandardInput.WriteLine($_) }
$proc.StandardInput.Close()

$stdout = $proc.StandardOutput.ReadToEnd()
$stderr = $proc.StandardError.ReadToEnd()
$proc.WaitForExit()

$stdout
if ($stderr) {
  "`n--- STDERR ---`n$stderr"
}
