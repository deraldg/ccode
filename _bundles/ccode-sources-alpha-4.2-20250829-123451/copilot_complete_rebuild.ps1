<#
.SYNOPSIS
  Cleans and builds a C++ project using CMake and MSBuild.
  Assumes CMakeLists.txt is in the root directory.
#>

[CmdletBinding()]
param(
  [string]$BuildDir = "build",
  [string]$Config   = "Release",
  [string]$Target   = ""  # Optional: specify target name
)

$ErrorActionPreference = "Stop"

# Resolve paths
$repoRoot = (Get-Location).Path
$absBuild = Join-Path $repoRoot $BuildDir

# Clean build directory
if (Test-Path $absBuild) {
  Write-Host "🧹 Cleaning build directory..." -ForegroundColor Yellow
  Remove-Item -Recurse -Force -Path $absBuild
}
New-Item -ItemType Directory -Path $absBuild | Out-Null

# Run CMake configuration
Write-Host "🔧 Configuring project with CMake..." -ForegroundColor Cyan
cmake -S $repoRoot -B $absBuild -DCMAKE_BUILD_TYPE=$Config

# Build with MSBuild
Write-Host "🚀 Building project..." -ForegroundColor Green
if ($Target) {
  cmake --build $absBuild --config $Config --target $Target
} else {
  cmake --build $absBuild --config $Config
}

Write-Host "✅ Build complete." -ForegroundColor Green