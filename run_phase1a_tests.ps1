\
Param(
  [string]$Config = "Release",
  [string]$BuildDir = "build",
  [string]$ExeRel = "build\Release\dottalkpp.exe",
  [switch]$Clean
)

$ErrorActionPreference = "Stop"

Write-Host "=== DotTalk++ Phase 1A Test Runner ==="

# Repo root check
if (-not (Test-Path -LiteralPath ".\CMakeLists.txt")) {
  Write-Error "Run this from your repo root (CMakeLists.txt must exist)."
  exit 1
}

# Optional clean
if ($Clean) {
  if (Test-Path -LiteralPath $BuildDir) {
    Write-Host "Removing $BuildDir ..."
    Remove-Item -Recurse -Force $BuildDir
  }
}

# Configure & build
Write-Host "Configuring..."
cmake -S . -B $BuildDir -DDOTTALK_WITH_INDEX=ON | Out-Host

Write-Host "Building ($Config)..."
cmake --build $BuildDir --config $Config --target dottalkpp | Out-Host

# Resolve exe path
$exe = Resolve-Path $ExeRel -ErrorAction SilentlyContinue
if (-not $exe) {
  # try alternative path
  $alt = Join-Path $BuildDir "$Config\dottalkpp.exe"
  if (Test-Path -LiteralPath $alt) { $exe = Resolve-Path $alt }
}
if (-not $exe) {
  Write-Error "Could not find dottalkpp.exe (looked at $ExeRel and $BuildDir\$Config)."
  exit 1
}

# Ensure test file exists next to this script if run from another dir
$testFile = Join-Path $PSScriptRoot "DotTalk_Phase1A_test_sequence.txt"
if (-not (Test-Path -LiteralPath $testFile)) {
  Write-Error "Missing test sequence file: $testFile"
  exit 1
}

Write-Host "Running tests with: $exe"
Get-Content $testFile | & $exe
