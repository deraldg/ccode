<#
.SYNOPSIS
  Collects all source and header files into a single flat file.
  Includes: *.cpp, *.c, *.hpp, *.h
  Scans recursively from current directory.
#>

[CmdletBinding()]
param(
  [string]$OutputFile = "flattened_sources.txt"
)

$ErrorActionPreference = "Stop"

# Define file types to include
$patterns = @("*.cpp", "*.c", "*.hpp", "*.h")

# Collect matching files recursively
$files = @()
foreach ($pat in $patterns) {
  $files += Get-ChildItem -Recurse -File -Include $pat -ErrorAction SilentlyContinue
}

if (-not $files) {
  Write-Warning "No source or header files found."
  return
}

# Create or overwrite output file
Set-Content -Path $OutputFile -Value "// === Flattened Source Dump ===`n"

foreach ($file in $files) {
  Add-Content -Path $OutputFile -Value "`n// --- File: $($file.FullName) ---`n"
  Get-Content -Path $file.FullName | Add-Content -Path $OutputFile
}

Write-Host "✅ Flattened source written to: $OutputFile"
Write-Host "  Files included: $($files.Count)"