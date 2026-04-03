<# 
.SYNOPSIS
  Search a repo for an exact text string (UTF-8/ANSI safe).

.EXAMPLE
  ./Find-RepoString.ps1 -Root "D:\code\ccode" `
    -Pattern ' Output fields are separated by ASCII Unit Separator (0x1F).'

.EXAMPLE
  ./Find-RepoString.ps1 -Root . -Include *.cpp,*.hpp,*.txt,*.md
#>

[CmdletBinding()]
param(
  [Parameter(Mandatory=$true)]
  [string]$Root,

  [string]$Pattern = ' Output fields are separated by ASCII Unit Separator (0x1F).',

  # File globs to include (defaults cover code + docs)
  [string[]]$Include = @('*.c','*.cpp','*.h','*.hpp','*.cs','*.py','*.js','*.ts','*.ps1','*.cmd','*.bat','*.txt','*.md'),

  # Globs to exclude (add build and vendor dirs)
  [string[]]$Exclude = @('*.exe','*.dll','*.lib','*.pdb','*.obj','*.bin','*.zip','*.7z','*.png','*.jpg','*.jpeg','*.gif','*.pdf'),

  [switch]$CaseSensitive,

  # Context lines around the match (like grep -nC)
  [int]$Context = 0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $Root)) {
  throw "Root path not found: $Root"
}

# Build Select-String parameters
$ssParams = @{
  Pattern       = [regex]::Escape($Pattern)   # exact string, not regex syntax
  Path          = (Get-ChildItem -LiteralPath $Root -Recurse -File -Include $Include -ErrorAction SilentlyContinue |
                   Where-Object { $Exclude -notcontains $_.Extension }) |
                   ForEach-Object { $_.FullName }
  SimpleMatch   = $true
  Encoding      = 'utf8'
  ErrorAction   = 'SilentlyContinue'
}

if ($CaseSensitive) { $ssParams['CaseSensitive'] = $true }
if ($Context -gt 0) { $ssParams['Context'] = @{ PreContext = $Context; PostContext = $Context } }

$matches = Select-String @ssParams

if (-not $matches) {
  Write-Host "No matches found." -ForegroundColor Yellow
  exit 1
}

# Pretty output: path:line | matched line
$matches | ForEach-Object {
  $rel = Resolve-Path -LiteralPath $_.Path
  Write-Host "$($rel):$($_.LineNumber)" -ForegroundColor Cyan
  Write-Host "  $($_.Line.Trim())"
  if ($Context -gt 0 -and $_.Context) {
    foreach ($pre in $_.Context.PreContext) { Write-Host "  $pre" -ForegroundColor DarkGray }
    foreach ($post in $_.Context.PostContext) { Write-Host "  $post" -ForegroundColor DarkGray }
  }
}

# Also emit objects for piping/saving if desired
$matches | Select-Object Path, LineNumber, Line | Out-Null
