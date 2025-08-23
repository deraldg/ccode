param(
  [ValidateSet('Debug','Release')] [string]$Config = 'Release',
  [switch]$Clean,
  [switch]$Regen,
  [string[]]$Args
)
$ErrorActionPreference = 'Stop'

# cd to repo root (folder containing this script)
Set-Location -LiteralPath (Split-Path -Parent $MyInvocation.MyCommand.Path)

if ($Clean -and (Test-Path .\build)) {
  Remove-Item -Recurse -Force .\build
}

if ($Regen -or -not (Test-Path .\build)) {
  cmake -S . -B build -DDOTTALK_WITH_INDEX=ON
}

cmake --build build --config $Config

# Resolve exe path (PS 5.1-safe)
$exeCandidates = @(
  "build\$Config\dottalkpp.exe",
  "build\dottalkpp.exe",
  "build\dottalkpp"   # non-Windows / MinGW case
)

$resolved = @()
foreach ($c in $exeCandidates) {
  try {
    $p = Resolve-Path -LiteralPath $c -ErrorAction Stop
    if ($p) { $resolved += $p.Path }
  } catch { }
}

if ($resolved.Count -gt 0) {
  $exe = $resolved[0]
} else {
  $found = Get-ChildItem -Path .\build -Recurse -Filter "dottalkpp*" -ErrorAction SilentlyContinue
  if ($found) {
    $exe = $found[0].FullName
  } else {
    Write-Error ("Could not find dottalkpp binary under .\build.")
    exit 1
  }
}

& $exe @Args
