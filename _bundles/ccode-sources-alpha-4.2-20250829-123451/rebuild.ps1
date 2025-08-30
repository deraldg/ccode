param(
  [switch]$Clean
)

$ErrorActionPreference = 'Stop'
$root     = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildDir = Join-Path $root 'build'
$exePath  = Join-Path $buildDir 'Release\dottalkpp.exe'

Write-Host ">>> Rebuild DotTalk++ (Release)"

# 1) Make sure no old EXE is running (prevents LNK1104)
Get-Process dottalkpp -ErrorAction SilentlyContinue | ForEach-Object {
  Write-Host ">>> Stopping running process PID=$($_.Id)"
  Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue
}
# Wait for file lock to clear (Defender/AV can hold it briefly)
$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
while (Test-Path $exePath) {
  try {
    $fs = [System.IO.File]::Open($exePath, 'Open', 'Read', 'None')
    $fs.Dispose()
    break
  } catch {
    Start-Sleep -Milliseconds 200
    if ($stopwatch.ElapsedMilliseconds -gt 8000) { break }
  }
}

# 2) Optional clean
if ($Clean) {
  Write-Host ">>> Removing build directory..."
  if (Test-Path $buildDir) { Remove-Item -Recurse -Force $buildDir }
}

# 3) Configure (idempotent)
Write-Host ">>> Running CMake configure..."
cmake -S $root -B $buildDir -G "Visual Studio 17 2022" -A x64 -DDOTTALK_WITH_INDEX=ON

# 4) Build
Write-Host ">>> Building target dottalkpp (Release)..."
cmake --build $buildDir --config Release -- /m

# 5) Run
if (Test-Path $exePath) {
  Write-Host ">>> Build complete."
  Write-Host "Executable: $exePath"
#  Write-Host ">>> Launching..."
#  & $exePath
} else {
  Write-Error "Build finished but EXE not found: $exePath"
}
