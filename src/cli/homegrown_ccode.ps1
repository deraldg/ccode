<# 
.SYNOPSIS
  Collects "homegrown" source files for DotTalk++ and friends from a ccode repo.

.DESCRIPTION
  Scans ONLY: src, include, bindings   (under the given repo root)
  Picks up:   C/C++ sources & headers, py files (bindings), CMake files, JSON manifests
  Adds root-level CMakeLists.txt and JSON (e.g., vcpkg.json) that matter to builds.
  Excludes:   build*/out/bin/obj/.git/_bundles/.venv/.vs/.vscode/third_party/external/etc.

  Output:     Creates a timestamped drop folder under <RepoRoot>\_drops\homegrown\
              Preserves folder structure (default). Optional -CreateFlat to flatten.
              Writes MANIFEST.txt with original→relative path mapping.

  Note:       Default behavior is files-only (no zip) to keep with two-step bundling.
              Use -Zip if you explicitly want a .zip.

.EXAMPLE
  # Default repo path; preserve structure; no zip
  .\homegrown_ccode.ps1

.EXAMPLE
  # Explicit root; flatten; zip
  .\homegrown_ccode.ps1 -RepoRoot 'C:\Users\deral\code\ccode' -CreateFlat -Zip -Label alpha-v7

#>

[CmdletBinding()]
param(
  [string]$RepoRoot = 'C:\Users\deral\code\ccode',
  [switch]$CreateFlat,
  [switch]$Zip,
  [string]$Label = ''
)

$ErrorActionPreference = 'Stop'

function Resolve-RepoRoot([string]$path) {
  if (-not $path) { throw "RepoRoot was null/empty." }
  $full = [System.IO.Path]::GetFullPath($path)
  if (-not (Test-Path -LiteralPath $full -PathType Container)) {
    throw "Repo root not found: $full"
  }
  return $full
}

$RepoRoot = Resolve-RepoRoot $RepoRoot

# --- Inputs we will scan
$ScanSubdirs = @('src','include','bindings')

# --- Allowed file sets
$CodeExts = @(
  '.c','.cc','.cpp','.cxx','.ixx',      # C/C++ sources (incl. C++ modules)
  '.h','.hh','.hpp','.hxx','.inl','.ipp'
)
$BindingExts = @('.py')                  # For pydottalk or helper Python in bindings
$CMakeNames = @('CMakeLists.txt')
$CMakeExts = @('.cmake')
$JsonExts  = @('.json')                  # e.g., vcpkg.json, config fragments

# --- Exclusion directory regexes (applied to FullName)
$ExcludeDirPatterns = @(
  '\.git($|\\)',
  '\bbuild[-\w]*($|\\)',
  '\b_bindings($|\\)',          # safety, if any local artifacts land here
  '\b_bindings-build($|\\)',
  '\b_bundles($|\\)',
  '\b_drops($|\\)',
  '\b_installs?($|\\)',
  '\bbin($|\\)',
  '\bobj($|\\)',
  '\bout($|\\)',
  '\bdist($|\\)',
  '\bexternal($|\\)',
  '\bthird[_\- ]?party($|\\)',
  '\bdeps?($|\\)',
  '\.venv($|\\)',
  '\.vs($|\\)',
  '\.vscode($|\\)',
  '\.idea($|\\)'
) | ForEach-Object { [regex]::new($_, 'IgnoreCase') }

function Test-ExcludedDir([string]$fullPath) {
  foreach ($re in $ExcludeDirPatterns) {
    if ($re.IsMatch($fullPath)) { return $true }
  }
  return $false
}

# --- Build the list of roots we will scan
$Roots = foreach ($sub in $ScanSubdirs) {
  $p = Join-Path -Path $RepoRoot -ChildPath $sub
  if (Test-Path -LiteralPath $p -PathType Container) { $p }
}

if (-not $Roots) {
  throw "None of the expected subdirs exist under $RepoRoot: $($ScanSubdirs -join ', ')"
}

# --- Collect candidates
$Files = New-Object System.Collections.Generic.List[System.IO.FileInfo]

# Helper to add files based on filter
function Add-MatchingFiles([string]$root) {
  # Recurse, but we’ll prune excluded dirs via a Where-Object on FullName
  Get-ChildItem -LiteralPath $root -File -Recurse -Force |
    Where-Object {
      # Keep only files whose directory path is not excluded
      -not (Test-ExcludedDir $_.DirectoryName)
    } |
    ForEach-Object {
      $ext = $_.Extension
      $name = $_.Name

      $isCode   = $CodeExts -contains $ext
      $isBind   = $BindingExts -contains $ext
      $isCMakeN = $CMakeNames -contains $name
      $isCMakeE = $CMakeExts -contains $ext
      $isJson   = $JsonExts -contains $ext

      if ($isCode -or $isBind -or $isCMakeN -or $isCMakeE -or $isJson) {
        $Files.Add($_)
      }
    }
}

foreach ($r in $Roots) { Add-MatchingFiles $r }

# Also pick up useful top-level control files (CMake & JSON) at repo root
$TopLevelExtra = @()
$TopLevelExtra += Get-ChildItem -LiteralPath $RepoRoot -File -Force -ErrorAction SilentlyContinue |
  Where-Object {
    ($CMakeNames -contains $_.Name) -or
    ($CMakeExts -contains $_.Extension) -or
    ($JsonExts  -contains $_.Extension -and $_.Name -match 'vcpkg|manifest|config|toolchain')
  }

foreach ($f in $TopLevelExtra) { $Files.Add($f) }

# --- De-dup & sort (by relative path)
$Rel = @{}
$Final = @()

foreach ($f in $Files) {
  $rel = [System.IO.Path]::GetRelativePath($RepoRoot, $f.FullName)
  if (-not $Rel.ContainsKey($rel)) {
    $Rel[$rel] = $true
    $Final += [pscustomobject]@{
      FileInfo = $f
      RelPath  = $rel
    }
  }
}

$Final = $Final | Sort-Object RelPath

if (-not $Final) {
  Write-Warning "No matching files found under: $($Roots -join ', ')"
  return
}

# --- Prepare output directory
$ts = Get-Date -Format 'yyyyMMdd-HHmmss'
$labelPiece = ($Label -and $Label.Trim()) ? "_$($Label.Trim())" : ''
$DropBase = Join-Path $RepoRoot '_drops\homegrown'
New-Item -ItemType Directory -Path $DropBase -Force | Out-Null

$OutDirName = "ccode_homegrown$labelPiece`_$ts"
$OutDir     = Join-Path $DropBase $OutDirName
New-Item -ItemType Directory -Path $OutDir -Force | Out-Null

# --- Copy files
$Copied = New-Object System.Collections.Generic.List[psobject]

foreach ($item in $Final) {
  $src = $item.FileInfo.FullName
  $rel = $item.RelPath

  if ($CreateFlat) {
    # Flattened name: replace path separators with '__'
    $safe = $rel -replace '[\\/]+','__'
    $dst  = Join-Path $OutDir $safe
    Copy-Item -LiteralPath $src -Destination $dst -Force
    $Copied.Add([pscustomobject]@{ Source=$src; Relative=$rel; Output=$dst })
  }
  else {
    # Preserve structure
    $dst = Join-Path $OutDir $rel
    $dstDir = Split-Path $dst -Parent
    if (-not (Test-Path -LiteralPath $dstDir)) {
      New-Item -ItemType Directory -Path $dstDir -Force | Out-Null
    }
    Copy-Item -LiteralPath $src -Destination $dst -Force
    $Copied.Add([pscustomobject]@{ Source=$src; Relative=$rel; Output=$dst })
  }
}

# --- Manifest
$Manifest = Join-Path $OutDir 'MANIFEST.txt'
"// Homegrown source drop for DotTalk++ and friends" | Set-Content -LiteralPath $Manifest -Encoding UTF8
"// RepoRoot: $RepoRoot" | Add-Content -LiteralPath $Manifest
"// DropDir:  $OutDir"   | Add-Content -LiteralPath $Manifest
"// Created:  $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss zzz')" | Add-Content -LiteralPath $Manifest
"// Options:  CreateFlat=$CreateFlat  Zip=$Zip  Label='$Label'" | Add-Content -LiteralPath $Manifest
"// Roots:    $($Roots -join ', ')" | Add-Content -LiteralPath $Manifest
"" | Add-Content -LiteralPath $Manifest
"// Files ($($Copied.Count)):" | Add-Content -LiteralPath $Manifest
foreach ($c in $Copied) {
  "REL: $($c.Relative)`nSRC: $($c.Source)`nOUT: $($c.Output)`n---" | Add-Content -LiteralPath $Manifest
}

# --- Optional zip (explicit only)
$ZipPath = Join-Path $DropBase ($OutDirName + '.zip')
if ($Zip) {
  if (Test-Path -LiteralPath $ZipPath) { Remove-Item -LiteralPath $ZipPath -Force }
  Compress-Archive -Path (Join-Path $OutDir '*') -DestinationPath $ZipPath -Force
}

# --- Summary
Write-Host "`nCollected $($Copied.Count) files into:" -ForegroundColor Green
Write-Host "  $OutDir"
if ($Zip) {
  Write-Host "Zip created:" -ForegroundColor Green
  Write-Host "  $ZipPath"
}
Write-Host "`nDone."
