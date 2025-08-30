<#
.SYNOPSIS
  Reconstructs original source files from a flattened source dump.
  Assumes each file is prefixed with: // --- File: <full path> ---
#>

[CmdletBinding()]
param(
  [string]$FlatFile = "flattened_sources.txt"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $FlatFile)) {
  Write-Error "Flat file '$FlatFile' not found."
  return
}

# Read all lines
$lines = Get-Content -Path $FlatFile

# Initialize state
$currentPath = $null
$currentBuffer = @()

foreach ($line in $lines) {
  if ($line -match '^// --- File: (.+) ---$') {
    # If we were collecting a previous file, write it out
    if ($currentPath -and $currentBuffer.Count -gt 0) {
      $dir = Split-Path -Path $currentPath -Parent
      if (-not (Test-Path $dir)) {
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
      }
      Set-Content -Path $currentPath -Value $currentBuffer
    }

    # Start collecting a new file
    $currentPath = $Matches[1]
    $currentBuffer = @()
  }
  elseif ($currentPath) {
    $currentBuffer += $line
  }
}

# Write the final file
if ($currentPath -and $currentBuffer.Count -gt 0) {
  $dir = Split-Path -Path $currentPath -Parent
  if (-not (Test-Path $dir)) {
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
  }
  Set-Content -Path $currentPath -Value $currentBuffer
}

Write-Host "✅ Reconstructed original files from: $FlatFile"