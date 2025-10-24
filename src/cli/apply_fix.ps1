
# apply_fix.ps1 - Copy fixed files into the destination repo.
param([string]$Dest)

if (-not $Dest) {
  Write-Error "Usage: .\apply_fix.ps1 -Dest <path-to-your-repo-root>"
  exit 1
}

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
Write-Host "Applying fix from: $here"
Write-Host "Destination repo: $Dest"

# Create src if needed
New-Item -ItemType Directory -Force -Path (Join-Path $Dest "src") | Out-Null

Copy-Item -Force (Join-Path $here "CMakeLists.txt") $Dest
Copy-Item -Force (Join-Path $here "src" "*") (Join-Path $Dest "src")

Write-Host "Done. Now build:"
Write-Host '  cmake -S . -B build -G "Visual Studio 17 2022" -A x64'
Write-Host '  cmake --build build --config Release'
Write-Host '  .\build\Release\dottalk_tui.exe'
