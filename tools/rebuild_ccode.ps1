<#
    Rebuild script for DotTalk++ (ccode)
    Usage: run from repo root →  .\tools\rebuild_ccode.ps1
#>

[CmdletBinding()]
param(
  [ValidateSet("Release","Debug")]
  [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$Repo     = Split-Path -Parent $MyInvocation.MyCommand.Path | Split-Path -Parent
$BuildDir = Join-Path $Repo "build"
$Target   = "dottalkpp"

Write-Host ">>> Rebuild DotTalk++ ($Config)" -ForegroundColor Cyan
Write-Host "Repo:   $Repo"
Write-Host "Build:  $BuildDir"
Write-Host "Target: $Target"
Write-Host ""

# 1) Kill common VS/ServiceHub lockers
$lockers = @(
  "devenv", "MSBuild", "VCTIP", "VCToolsService",
  "ServiceHub.Host", "ServiceHub.VSDetouredHost",
  "ServiceHub.RoslynCodeAnalysisService",
  "ServiceHub.Intellicode", "ServiceHub.IdentityHost"
)

foreach ($name in $lockers) {
  Get-Process -Name $name -ErrorAction SilentlyContinue | ForEach-Object {
    Write-Host (" - Stopping {0} (PID {1})" -f $_.ProcessName, $_.Id)
    Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue
  }
}

Start-Sleep -Seconds 1

# 2) Remove old build directory
if (Test-Path -LiteralPath $BuildDir) {
  Write-Host ">>> Removing build directory..." -ForegroundColor Cyan
  Remove-Item -LiteralPath $BuildDir -Recurse -Force
}

# 3) Quiet MSVC UTF-8 warnings
$env:CL = "/utf-8"

# 4) Configure
Write-Host ">>> Running CMake configure..." -ForegroundColor Cyan
cmake -S $Repo -B $BuildDir -G "Visual Studio 17 2022" -A x64 -DDOTTALK_WITH_INDEX=ON

# 5) Build
Write-Host ">>> Building target $Target ($Config)..." -ForegroundColor Cyan
cmake --build $BuildDir --config $Config --target $Target

# 6) Locate exe
$ExePath = Join-Path $BuildDir "$Config\$Target.exe"
if (Test-Path -LiteralPath $ExePath) {
  Write-Host ">>> Build complete." -ForegroundColor Green
  Write-Host "Executable: $ExePath" -ForegroundColor Green
  Write-Host ">>> Launch with:" -ForegroundColor Cyan
  Write-Host "    & `"$ExePath`""
} else {
  throw "Build finished but $ExePath not found."
}
