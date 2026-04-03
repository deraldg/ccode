<#  datarun.ps1 — Launch DotTalk++ with CWD set to dottalkpp\data
    Place in: D:\code\ccode\dottalkpp\
    Usage:
      pwsh -File .\datarun.ps1                # run using bin\ if present, else best found
      pwsh -File .\datarun.ps1 -CopyToBin     # one-time copy to bin\ (if not already there)
      pwsh -File .\datarun.ps1 -Fresh         # ALWAYS copy newest build → bin\ before run
#>

[CmdletBinding()]
param(
  [switch]$CopyToBin,
  [Alias('SyncBin')][switch]$Fresh
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# --- Paths -------------------------------------------------------------------
$root = Split-Path -Parent $MyInvocation.MyCommand.Path   # ...\dottalkpp
$repo = Split-Path -Parent $root                          # ...\ccode
$data = Join-Path $root 'data'
$bin  = Join-Path $root 'bin'
$exeInBin = Join-Path $bin 'dottalkpp.exe'

# Candidate exe locations (common build trees)
$candidateGlobs = @(
  (Join-Path $repo 'build\src\Release\dottalkpp.exe'),
  (Join-Path $repo 'build\src\Debug\dottalkpp.exe'),
  (Join-Path $repo 'build-msvc\src\Release\dottalkpp.exe'),
  (Join-Path $repo 'build-msvc\src\Debug\dottalkpp.exe'),
  (Join-Path $repo 'build-tests\src\Release\dottalkpp.exe'),
  (Join-Path $repo 'build-tests\src\Debug\dottalkpp.exe'),
  (Join-Path $repo 'build-legacy\src\Release\dottalkpp.exe'),
  (Join-Path $repo 'build-legacy\src\Debug\dottalkpp.exe')
)

# Helper: return newest existing exe among candidates and bin\
function Get-NewestExe {
  $cands =
    @($exeInBin) + $candidateGlobs |
    Where-Object { Test-Path $_ } |
    ForEach-Object {
      [pscustomobject]@{
        Path = $_
        Time = (Get-Item $_).LastWriteTimeUtc
      }
    }

  if (-not $cands) { return $null }
  $cands | Sort-Object Time -Descending | Select-Object -First 1
}

# Resolve executable and optionally sync to bin
$newest = Get-NewestExe
if (-not $newest) {
  throw 'dottalkpp.exe not found. Build the project or copy the exe into bin\.'
}

# Ensure bin\ exists
if (-not (Test-Path $bin)) { New-Item -ItemType Directory -Path $bin | Out-Null }

# Fresh: always copy newest into bin before launching
if ($Fresh) {
  Copy-Item -LiteralPath $newest.Path -Destination $exeInBin -Force
}

# CopyToBin: one-time copy if not launching from bin already
elseif ($CopyToBin -and ($newest.Path -ne $exeInBin)) {
  Copy-Item -LiteralPath $newest.Path -Destination $exeInBin -Force
}

# Choose exe to run: prefer bin if present after any copy, otherwise newest found
$exe = if (Test-Path $exeInBin) { $exeInBin } else { $newest.Path }

# --- Ensure sqlite3.dll alongside the exe (if dynamically linked) ------------
$exeDir = Split-Path -Parent $exe
$sqliteDll = Join-Path $exeDir 'sqlite3.dll'
if (-not (Test-Path $sqliteDll)) {
  $guess = Get-ChildItem -Path (Join-Path $repo 'vcpkg_installed') -Recurse -Filter 'sqlite3.dll' -ErrorAction SilentlyContinue |
           Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
  if ($guess) {
    Copy-Item -LiteralPath $guess.FullName -Destination $sqliteDll -Force
  }
}

# --- Prepare runtime workspace ------------------------------------------------
foreach ($d in 'out','tmp','logs','dbf','indexes','scripts','schemas','help') {
  $p = Join-Path $data $d
  if (-not (Test-Path $p)) { New-Item -ItemType Directory -Path $p | Out-Null }
}

# --- Launch from data/ so paths are short (dbf\..., out\...) ------------------
Set-Location $data
Write-Host "Launching: $exe" -ForegroundColor Cyan
& $exe
$code = $LASTEXITCODE
if ($code -ne $null) { exit $code }
