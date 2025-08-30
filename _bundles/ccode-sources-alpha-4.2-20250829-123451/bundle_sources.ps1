<# 
.SYNOPSIS
  Bundle only source + build scripts into a lean ZIP.
  Includes:  *.cpp, *.hpp, CMakeLists.txt, Makefile/makefile, *.ps1
  Excludes:  build artifacts and VCS folders.
#>

[CmdletBinding()]
param(
  [string]$OutDir = "_bundles",
  [string]$Label  = "alpha-4.2"
)

$ErrorActionPreference = "Stop"

# Resolve an absolute output path under the current repo
$repoRoot = (Get-Location).Path
$absOut   = Join-Path $repoRoot $OutDir

# Ensure output directory exists
if (-not (Test-Path -LiteralPath $absOut)) {
  New-Item -ItemType Directory -Path $absOut | Out-Null
}

# Timestamp and zip name
$ts      = Get-Date -Format "yyyyMMdd-HHmmss"
$zipName = "ccode-sources-$Label-$ts.zip"
$zipPath = Join-Path $absOut $zipName

# Patterns to include
$includePatterns = @(
  "*.cpp","*.hpp",
  "CMakeLists.txt",
  "Makefile","makefile",
  "*.ps1"
)

# Directories to exclude (regex, case-insensitive)
$excludeDirRegex = '(?i)\\(?:\.git|\.vs|build|_ccode_build|_pcode_build|x64|x86|Debug|Release|CMakeFiles|out|bin|obj)(\\|$)'

# Collect files per pattern, then de-dup and exclude bad dirs
$files = @()
foreach ($pat in $includePatterns) {
  $files += Get-ChildItem -Path $repoRoot -Recurse -File -Filter $pat -ErrorAction SilentlyContinue
}
$files = $files |
  Sort-Object FullName -Unique |
  Where-Object { $_.FullName -notmatch $excludeDirRegex }

if (-not $files) {
  Write-Warning "No matching files found."
  return
}

# Create the zip
Add-Type -AssemblyName System.IO.Compression.FileSystem
if (Test-Path $zipPath) { Remove-Item $zipPath -Force }

$zip = [System.IO.Compression.ZipFile]::Open($zipPath, 'Create')
try {
  foreach ($f in $files) {
    # store path relative to repo root
    $rel = Resolve-Path -LiteralPath $f.FullName | ForEach-Object {
      $_.Path.Substring($repoRoot.Length).TrimStart('\','/')
    }
    [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
      $zip, $f.FullName, $rel, [System.IO.Compression.CompressionLevel]::Optimal
    ) | Out-Null
  }
}
finally {
  $zip.Dispose()
}

Write-Host "Created bundle:" -ForegroundColor Green
Write-Host "  $zipPath"
Write-Host ("  Files included: {0}" -f $files.Count)
