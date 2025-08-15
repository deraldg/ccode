<# 
Zip only .cpp/.hpp files from your repo, preserving folder structure.
Usage (PowerShell):
  .\zip_sources.ps1 -TopDir "C:\Users\deral\code\ccode" -ZipFile "C:\Users\deral\code\SelectedFiles.zip"
#>

param(
  [Parameter(Mandatory=$true)]
  [string]$TopDir,

  [Parameter(Mandatory=$true)]
  [string]$ZipFile
)

# --- Normalize & validate paths ---
$TopDir = (Resolve-Path -LiteralPath $TopDir).Path
if (-not (Test-Path -LiteralPath $TopDir)) {
  Write-Error "TopDir does not exist: $TopDir"
  exit 1
}

$ZipDir = Split-Path -Path $ZipFile -Parent
if (-not (Test-Path -LiteralPath $ZipDir)) {
  New-Item -ItemType Directory -Path $ZipDir | Out-Null
}

# --- Temp workspace ---
$TempDir = Join-Path -Path $env:TEMP -ChildPath ("CppHppZipTemp_" + [guid]::NewGuid().ToString("N"))
New-Item -Path $TempDir -ItemType Directory | Out-Null

# --- Choose what to include/exclude ---
# Note: -Include only works with -Recurse when -Path contains a wildcard; we’ll use -Filter for reliability.
# We run two passes (cpp + hpp) and merge results.
$excludeDirs = @(
  '\.git($|\\)',
  '[\\/]build($|\\)',
  '[\\/]out($|\\)',
  '[\\/]externals[\\/].*\\b(xindex_scaffold)[\\/]?bin', # example; adjust as you want
  '[\\/]Debug($|\\)',
  '[\\/]Release($|\\)'
)

function Should-ExcludePath([string]$p) {
  foreach ($rx in $excludeDirs) {
    if ($p -imatch $rx) { return $true }
  }
  return $false
}

$files = @()
$files += Get-ChildItem -Path $TopDir -Recurse -File -Filter *.cpp | Where-Object { -not (Should-ExcludePath $_.FullName) }
$files += Get-ChildItem -Path $TopDir -Recurse -File -Filter *.hpp | Where-Object { -not (Should-ExcludePath $_.FullName) }

if ($files.Count -eq 0) {
  Write-Warning "No .cpp/.hpp files found under $TopDir"
  Remove-Item -Path $TempDir -Recurse -Force
  exit 0
}

# --- Copy while preserving directory structure relative to TopDir ---
$prefixLen = $TopDir.TrimEnd('\').Length
$copied = 0
foreach ($f in $files) {
  $relative = $f.FullName.Substring($prefixLen).TrimStart('\','/')
  $destPath = Join-Path -Path $TempDir -ChildPath $relative
  $destDir  = Split-Path -Path $destPath -Parent
  New-Item -Path $destDir -ItemType Directory -Force | Out-Null
  Copy-Item -Path $f.FullName -Destination $destPath -Force
  $copied++
  if ($copied % 50 -eq 0) { Write-Host "  Copied $copied files..." }
}

# --- Create zip ---
if (Test-Path -LiteralPath $ZipFile) {
  Remove-Item -LiteralPath $ZipFile -Force
}
Compress-Archive -Path (Join-Path $TempDir '*') -DestinationPath $ZipFile -CompressionLevel Optimal -Force

# --- Clean up ---
Remove-Item -Path $TempDir -Recurse -Force

Write-Host "✅ Zip created:" -NoNewline
Write-Host " $ZipFile"
Write-Host "   Files included: $copied"
