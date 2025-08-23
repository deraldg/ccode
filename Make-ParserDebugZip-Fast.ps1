<#
Make-ParserDebugZip-Fast.ps1
Ultra-fast: bundle ONLY git-tracked files using `git archive`.
Creates releases\parser-debug\parser-debug-git-<rev>-<timestamp>.zip
#>


[CmdletBinding()]
param(
[string]$OutDir = ".\releases\parser-debug"
)


$ErrorActionPreference = "Stop"


# Ensure we are in a git repo
try { $null = git rev-parse --is-inside-work-tree 2>$null } catch {
throw "Not a git repository. Run from your repo root."
}


$rev = (git rev-parse --short HEAD).Trim()
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$newDir = Join-Path $OutDir .
New-Item -ItemType Directory -Path $newDir -Force | Out-Null
$zip = Join-Path $newDir ("parser-debug-git-$rev-$stamp.zip")


# Create the archive
& git archive -o $zip --format=zip HEAD


Write-Host "Git archive created:" -ForegroundColor Green
Write-Host " $zip"