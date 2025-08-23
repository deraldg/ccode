<#
  Export wrapper that honors a target path by renaming the produced CSV.
  Usage:
    .\tools\export_ccode.ps1 -Table TEMP -Out TEMP_out.csv [-Config Release|Debug]
#>

[CmdletBinding()]
param(
  [Parameter(Mandatory=$true)][string]$Table,
  [Parameter(Mandatory=$true)][string]$Out,
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

# Run export (program writes <Table>.csv)
$cmds = @(
  "USE $Table",
  "EXPORT $Table TO $Table.csv",
  "QUIT"
) -join "`n"

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

$produced = "$Table.csv"
if (-not (Test-Path -LiteralPath $produced)) {
  throw "Export did not produce $produced. Output:`n$stdout`n$stderr"
}

Move-Item -Force -LiteralPath $produced -Destination $Out
Write-Host "Exported to $Out" -ForegroundColor Green
