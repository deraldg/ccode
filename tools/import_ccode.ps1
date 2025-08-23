<#
  Import wrapper that honors FROM <path> and LIMIT by pre-trimming the CSV.
  Usage:
    .\tools\import_ccode.ps1 -Table T2 -From TEMP.csv -Limit 3 -Fields '(FIRST_NAME AS Y, ID AS X)' [-Config Release|Debug]
#>

[CmdletBinding()]
param(
  [Parameter(Mandatory=$true)][string]$Table,
  [Parameter(Mandatory=$true)][string]$From,
  [int]$Limit = 0,
  [string]$Fields = "",
  [ValidateSet("Release","Debug")][string]$Config = "Release"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$Repo     = Split-Path -Parent $MyInvocation.MyCommand.Path | Split-Path -Parent
$BuildDir = Join-Path $Repo "build"
$ExePath  = Join-Path $BuildDir "$Config\dottalkpp.exe"

if (-not (Test-Path -LiteralPath $ExePath)) {
  throw "Executable not found: $ExePath`nRun .\tools\rebuild_ccode.ps1 first."
}
if (-not (Test-Path -LiteralPath $From)) {
  throw "CSV not found: $From"
}

# Prepare temp CSV honoring LIMIT if provided
$tmp = [System.IO.Path]::ChangeExtension([System.IO.Path]::GetTempFileName(), ".csv")

$lines = Get-Content -LiteralPath $From
if ($Limit -gt 0) {
  if ($lines.Count -le 1) { throw "CSV seems empty: $From" }
  $header = $lines[0]
  $body = $lines | Select-Object -Skip 1 | Select-Object -First $Limit
  $header | Set-Content -LiteralPath $tmp -Encoding UTF8
  $body   | Add-Content -LiteralPath $tmp -Encoding UTF8
} else {
  $lines | Set-Content -LiteralPath $tmp -Encoding UTF8
}

# Build IMPORT command
$fieldsPart = $Fields.Trim()
if ($fieldsPart.Length -gt 0) {
  $fieldsPart = " FIELDS " + $fieldsPart
}

$cmds = @(
  "USE $Table",
  "IMPORT $Table FROM $([System.IO.Path]::GetFileName($tmp))$fieldsPart",
  "QUIT"
) -join "`n"

# Copy temp CSV to cwd (program likely reads relative path)
Copy-Item -Force -LiteralPath $tmp -Destination (Split-Path -Leaf $tmp)

# Launch program
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

Write-Host "Import completed (source: $From, limit: $Limit)" -ForegroundColor Green
if ($stderr) { Write-Host $stderr -ForegroundColor Yellow }
