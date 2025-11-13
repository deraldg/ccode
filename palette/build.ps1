# ===== File: build.ps1  (run from repo root) =====
param(
  [ValidateSet("msvc","mingw")][string]$Toolset = "msvc",
  [ValidateSet("Release","Debug","RelWithDebInfo")][string]$Config = "RelWithDebInfo",
  [string]$TVRoot = ""  # optional override of TVISION_ROOT
)

$ErrorActionPreference = "Stop"

# Repo root (works whether invoked as script or dot-sourced)
$root  = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }
$build = Join-Path $root "build"

# Hard reset any accidental in-source configuration
$inSourceCache = Join-Path $root "CMakeCache.txt"
$inSourceFiles = Join-Path $root "CMakeFiles"
$depsDir       = Join-Path $root "_deps"

if (Test-Path $inSourceCache) { Remove-Item $inSourceCache -Force }
if (Test-Path $inSourceFiles) { Remove-Item $inSourceFiles -Recurse -Force }
if (Test-Path $depsDir)       { Remove-Item $depsDir -Recurse -Force }

# Fresh out-of-source build dir
if (Test-Path $build) { Remove-Item $build -Recurse -Force }
New-Item -ItemType Directory -Path $build | Out-Null

# Generator + toolchain args
$genArgs = @()
$cmakeArgs = @("-DCMAKE_BUILD_TYPE=$Config")
if ($TVRoot) { $cmakeArgs += "-DTVISION_ROOT=$TVRoot" }

if ($Toolset -eq "msvc") {
  $hasNinja = (Get-Command ninja -ErrorAction SilentlyContinue) -ne $null
  if ($hasNinja) { $genArgs = @("-G","Ninja") }
  else           { $genArgs = @("-G","Visual Studio 17 2022","-A","x64") }
} else {
  $genArgs   = @("-G","Ninja")
  $cmakeArgs += @("-DCMAKE_C_COMPILER=gcc","-DCMAKE_CXX_COMPILER=g++")
}

# Configure OUT-OF-SOURCE explicitly
& cmake @genArgs @cmakeArgs -S "$root" -B "$build"

# Build using the build dir explicitly (no Push-Location needed)
& cmake --build "$build" --config $Config -- -v

Write-Host "`nOutput:" -ForegroundColor Cyan
Get-ChildItem "$build/fox_palette*" -Recurse -Include fox_palette.exe,fox_palette | ForEach-Object { $_.FullName }
