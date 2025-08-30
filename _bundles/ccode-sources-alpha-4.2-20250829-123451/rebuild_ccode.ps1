param(
  [ValidateSet('Debug','Release')]
  [string]$Config = 'Release',
  [string]$Generator = 'Visual Studio 17 2022',
  [ValidateSet('Win32','x64','ARM64')]
  [string]$Arch = 'x64',
  [switch]$Clean
)

$ErrorActionPreference = 'Stop'

# Repo root = folder that contains this script (…\code\ccode)
$RepoRoot = (Resolve-Path $PSScriptRoot).Path
$BuildDir = Join-Path $RepoRoot 'build'
$Target   = 'dottalkpp'
$ExePath  = Join-Path $BuildDir "$Config\dottalkpp.exe"

Write-Host ">>> Rebuild DotTalk++ ($Config)"
Write-Host "Repo:   $RepoRoot"
Write-Host "Build:  $BuildDir"
Write-Host "Target: $Target"
Write-Host ""

# Optional: kill IntelliSense helper that sometimes locks files
Get-Process vctip -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue | Out-Null

# Clean build dir if requested or missing
if ($Clean -and (Test-Path $BuildDir)) {
  Write-Host ">>> Removing build directory..."
  Remove-Item -Recurse -Force $BuildDir
}
if (-not (Test-Path $BuildDir)) {
  New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

Write-Host ">>> Running CMake configure..."
cmake -S $RepoRoot -B $BuildDir -G $Generator -A $Arch | Write-Host

Write-Host ">>> Building target $Target ($Config)..."
cmake --build $BuildDir --config $Config --target $Target | Write-Host

if (-not (Test-Path $ExePath)) {
  throw "Build finished but $ExePath not found."
}

Write-Host "Exe: $ExePath"
