.\bundle_python_sources.ps1 -OutDir _bundles -Label py-alpha

<#
.SYNOPSIS
  Bundle only Python source + project scripts into a lean ZIP.
  Includes:  *.py, pyproject.toml, requirements*.txt, setup.*, Pipfile, poetry.lock,
              uv.lock, .flake8, .pylintrc, mypy.ini, pytest.ini, tox.ini, *.ipynb (optional),
              plus *.ps1 build helpers.
  Excludes:  virtualenvs, caches, build/dist artifacts, VCS folders.
#>

[CmdletBinding()]
param(
  [string]$OutDir = "_bundles",
  [string]$Label  = "py-sources",
  [switch]$IncludeNotebooks # add -IncludeNotebooks to include *.ipynb
)

$ErrorActionPreference = "Stop"

# Resolve repo root and output dir
$repoRoot = (Get-Location).Path
$absOut   = Join-Path $repoRoot $OutDir
if (-not (Test-Path -LiteralPath $absOut)) {
  New-Item -ItemType Directory -Path $absOut | Out-Null
}

# Timestamp and zip name
$ts      = Get-Date -Format "yyyyMMdd-HHmmss"
$zipName = "python-sources-$Label-$ts.zip"
$zipPath = Join-Path $absOut $zipName

# Include patterns (Python-centric)
$includePatterns = @(
  "*.py",
  "pyproject.toml",
  "requirements.txt", "requirements-*.txt",
  "setup.py", "setup.cfg",
  "Pipfile", "Pipfile.lock",
  "poetry.lock",
  "uv.lock",
  ".flake8", ".pylintrc",
  "mypy.ini", "pytest.ini", "tox.ini",
  "*.ps1"
)
if ($IncludeNotebooks) { $includePatterns += "*.ipynb" }

# Exclude directories (regex, case-insensitive)
# - virtualenvs: .venv, venv
# - caches/build: __pycache__, .mypy_cache, .pytest_cache, .tox, .eggs, build, dist, .ipynb_checkpoints
# - VCS/IDE: .git, .svn, .hg, .vs, .idea
$excludeDirRegex = '(?i)\\(?:\.git|\.svn|\.hg|\.vs|\.idea|__pycache__|\.mypy_cache|\.pytest_cache|\.tox|\.eggs|build|dist|\.ipynb_checkpoints|\.venv|venv|_ccode_build|_pcode_build|x64|x86|Debug|Release|CMakeFiles|out|bin|obj)(\\|$)'

# Collect files per pattern, then de-dup and exclude unwanted dirs
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
    # path relative to repo root
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
