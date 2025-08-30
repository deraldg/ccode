param(
  [string]$Root = ".",
  [string[]]$Extensions = @(".h",".hpp",".hh",".cpp",".cxx",".cc",".ipp"),
  [switch]$Name,
  [switch]$Content,
  [switch]$CmdHandlers,
  [switch]$DbArea,
  [switch]$All
)

if ($All) { $Name = $Content = $CmdHandlers = $DbArea = $true }

# Collect source files
$files = Get-ChildItem -Path $Root -Recurse -File |
         Where-Object { $Extensions -contains $_.Extension }

if (-not $files) { Write-Host "No files found under $Root" -ForegroundColor Yellow; exit 0 }

$exitFlag = $false

# --- 1) Duplicate filenames (exact name) -------------------------------
if ($Name) {
  $nameGroups = $files | Group-Object Name | Where-Object { $_.Count -gt 1 }
  if ($nameGroups) {
    $exitFlag = $true
    Write-Host "== Duplicate filenames (exact match) ==" -ForegroundColor Cyan
    foreach ($g in $nameGroups) {
      Write-Host ("{0}  ({1} places)" -f $g.Name, $g.Count) -ForegroundColor Yellow
      $g.Group.FullName | Sort-Object | ForEach-Object { "  - $_" } | Write-Output
      ""
    }
  } else {
    Write-Host "No exact filename duplicates." -ForegroundColor DarkGray
  }

  # Normalize prefixes like src_/src_cli_
  $normalized = $files | ForEach-Object {
    $norm = $_.Name -replace '^(src_cli_|src_)',''
    [pscustomobject]@{ Norm = $norm; File = $_.FullName }
  }
  $normGroups = $normalized | Group-Object Norm | Where-Object { $_.Count -gt 1 }
  if ($normGroups) {
    $exitFlag = $true
    Write-Host "== Duplicate filenames after prefix normalization (src_/src_cli_) ==" -ForegroundColor Cyan
    foreach ($g in $normGroups) {
      Write-Host ("{0}  ({1} places)" -f $g.Name, $g.Count) -ForegroundColor Yellow
      $g.Group.File | Sort-Object | ForEach-Object { "  - $_" } | Write-Output
      ""
    }
  } else {
    Write-Host "No normalized filename duplicates." -ForegroundColor DarkGray
  }
}

# --- 2) Duplicate content (hash) --------------------------------------
if ($Content) {
  $hashes = $files | Get-FileHash -Algorithm SHA256
  $dupes = $hashes | Group-Object Hash | Where-Object { $_.Count -gt 1 }
  if ($dupes) {
    $exitFlag = $true
    Write-Host "== Duplicate content (SHA256) ==" -ForegroundColor Cyan
    foreach ($d in $dupes) {
      Write-Host ("Hash: {0}" -f $d.Name) -ForegroundColor Yellow
      $d.Group.Path | Sort-Object | ForEach-Object { "  - $_" } | Write-Output
      ""
    }
  } else {
    Write-Host "No byte-identical file duplicates." -ForegroundColor DarkGray
  }
}

# --- 3) Duplicate cmd_* handlers --------------------------------------
if ($CmdHandlers) {
  $pattern = 'void\s+cmd_([A-Za-z0-9_]+)\s*\('
  $hits = foreach ($f in $files) {
    $m = Select-String -Path $f.FullName -Pattern $pattern -AllMatches -SimpleMatch:$false
    foreach ($mm in $m) {
      foreach ($g in $mm.Matches) {
        [pscustomobject]@{
          Command  = $g.Groups[1].Value
          File     = $f.FullName
          Line     = $mm.LineNumber
          LineText = $mm.Line.Trim()
        }
      }
    }
  }
  $dupCmds = $hits | Group-Object Command | Where-Object { $_.Count -gt 1 }
  if ($dupCmds) {
    $exitFlag = $true
    Write-Host "== Duplicate cmd_* handlers ==" -ForegroundColor Cyan
    foreach ($g in $dupCmds) {
      Write-Host ("cmd_{0}" -f $g.Name) -ForegroundColor Yellow
      $g.Group | Sort-Object File, Line | Format-Table -AutoSize File, Line | Out-String -Width 500 | Write-Output
      ""
    }
  } else {
    Write-Host "No duplicate cmd_* handlers." -ForegroundColor DarkGray
  }
}

# --- 4) Conflicting DbArea definitions/aliases -------------------------
if ($DbArea) {
  $typePat    = '^\s*(class|struct)\s+DbArea\b'
  $usingPat   = '^\s*using\s+DbArea\b'
  $typedefPat = '^\s*typedef\b.*\bDbArea\b'

  $typeHits = @()
  foreach ($f in $files) {
    $lines = Get-Content -Path $f.FullName -ErrorAction SilentlyContinue
    if (-not $lines) { continue }
    for ($i = 0; $i -lt $lines.Length; $i++) {
      $ln = $lines[$i]
      if ($ln -match $typePat -or $ln -match $usingPat -or $ln -match $typedefPat) {
        $typeHits += [pscustomobject]@{
          Path = $f.FullName
          Line = $i + 1
          Text = $ln.Trim()
        }
      }
    }
  }

  if ($typeHits) {
    $exitFlag = $true
    Write-Host "== Potentially conflicting DbArea declarations/aliases ==" -ForegroundColor Cyan
    $typeHits | Sort-Object Path, Line | Format-Table -AutoSize Path, Line, Text | Out-String -Width 500 | Write-Output
    ""
  } else {
    Write-Host "No obvious DbArea type/alias duplicates." -ForegroundColor DarkGray
  }
}

if ($exitFlag) { exit 1 } else { exit 0 }
