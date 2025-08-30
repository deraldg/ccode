# build.ps1
[CmdletBinding()]
param(
  [ValidateSet('Debug','Release')]
  [string]$Config = 'Release',

  # Optional: override build dir; defaults to "<repo>/build"
  [string]$BuildDir,

  # Pass-thru args to the exe when -Run is used
  [string[]]$Args,

  # If set, run the built exe after building
  [switch]$Run
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# Script root (repo root if this script lives there)
$RepoRoot = $PSScriptRoot
if (-not $RepoRoot) { $RepoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path }

# Default build dir if not provided
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
  $BuildDir = Join-Path $RepoRoot 'build'
}

Write-Host "RepoRoot: $RepoRoot"
Write-Host "BuildDir: $BuildDir"
Write-Host "Config:   $Config"

# Configure if needed
if (-not (Test-Path $BuildDir)) {
  & cmake -S $RepoRoot -B $BuildDir
}

# Build
& cmake --build $BuildDir --config $Config --target dottalkpp

# Resolve exe location for various generators/layouts
$exeCandidates = @(
  (Join-Path $BuildDir "bin\$Config\dottalkpp.exe"),
  (Join-Path $BuildDir "$Config\dottalkpp.exe"),
  (Join-Path $BuildDir "Release\dottalkpp.exe"),
  (Join-Path $BuildDir "Debug\dottalkpp.exe"),
  (Join-Path $BuildDir "dottalkpp.exe")
)

$exe = $null
foreach ($c in $exeCandidates) {
  if (Test-Path $c) { $exe = (Resolve-Path $c).Path; break }
}

if (-not $exe) {
  Write-Warning "Built OK, but couldn't find dottalkpp.exe in expected locations."
  Write-Host  "Checked:"; $exeCandidates | ForEach-Object { Write-Host "  $_" }
  return
}

Write-Host "Exe: $exe"

if ($Run) {
  # Run from the data directory if you prefer, otherwise use $RepoRoot
  Push-Location (Join-Path $RepoRoot 'data'); try {
    & $exe @Args
  } finally {
    Pop-Location
  }
}
