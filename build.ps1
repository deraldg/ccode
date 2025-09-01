Param(
  [ValidateSet('Debug','Release')][string]$Config = 'Release',
  [switch]$UseNinja
)

$ErrorActionPreference = 'Stop'
$RepoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $RepoRoot

$BuildDir = Join-Path $RepoRoot 'build'
Write-Host "RepoRoot: $RepoRoot"
Write-Host "BuildDir: $BuildDir"
Write-Host "Config:   $Config"

# --- 1) Detect mixed-environment CMake cache and clean ---
$Cache = Join-Path $BuildDir 'CMakeCache.txt'
if (Test-Path $Cache) {
  $cacheText = Get-Content $Cache -Raw
  $hasWSLPath = $cacheText -match '/mnt/' -or $cacheText -match '/home/'
  $hasWinPath = $cacheText -match '^[A-Z]:\\'
  $cwdIsWin   = $PWD.Path -match '^[A-Z]:\\'

  if ($cwdIsWin -and $hasWSLPath) {
    Write-Warning "CMakeCache points to WSL paths but you're in Windows PowerShell. Cleaning build/…"
    Remove-Item -Recurse -Force $BuildDir
  }
  elseif (-not $cwdIsWin -and $hasWinPath) {
    Write-Warning "CMakeCache points to Windows paths but you're in WSL. Cleaning build/…"
    Remove-Item -Recurse -Force $BuildDir
  }
}

# Ensure build dir exists
if (!(Test-Path $BuildDir)) { New-Item -ItemType Directory -Path $BuildDir | Out-Null }

# --- 2) Configure ---
$generator = if ($UseNinja) { 'Ninja' } else { 'Visual Studio 17 2022' }

$configureArgs = @('-S', $RepoRoot, '-B', $BuildDir, '-G', $generator, '-D', "CMAKE_BUILD_TYPE=$Config")
if (-not $UseNinja) { $configureArgs += @('-A','x64') }

Write-Host ">>> Running CMake configure..."
cmake @configureArgs

# --- 3) Build ---
Write-Host ">>> Building target dottalkpp..."
if ($UseNinja) {
  cmake --build $BuildDir --target dottalkpp
} else {
  cmake --build $BuildDir --config $Config --target dottalkpp
}

# --- 4) Locate executable ---
$CandidatePaths = @(
  (Join-Path $BuildDir "bin\$Config\dottalkpp.exe"),
  (Join-Path $BuildDir "$Config\dottalkpp.exe"),
  (Join-Path $BuildDir "dottalkpp.exe"),
  (Join-Path $BuildDir "Release\dottalkpp.exe"),
  (Join-Path $BuildDir "Debug\dottalkpp.exe")
)

$exe = $null
foreach ($p in $CandidatePaths) {
  if (Test-Path $p) { $exe = $p; break }
}

if ($exe) {
  Write-Host "Built OK: $exe"
} else {
  Write-Warning "Built, but couldn't find dottalkpp.exe in expected locations."
  Write-Host "Checked:"
  $CandidatePaths | ForEach-Object { Write-Host "  $_" }
}
