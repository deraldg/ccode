#Requires -Version 5.1
<#
  Purpose: Normalize TAG usage and add canonical "SET ORDER TO" support.
  What it does:
    1) command_registry.cpp: add an alias so "SET ORDER TO" maps to the same handler as "SETORDER"
    2) cmd_help.cpp: document the canonical syntax for INDEX/SET ORDER/SEEK
    3) cmd_status.cpp & cmd_struct.cpp: change banner to "Active tag: ..."
    4) Build (Release)

  Safe: makes one-time *.bak backups of touched files.
#>

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ----- CONFIG -----
$Root = Split-Path -Parent $PSCommandPath
if (-not (Test-Path "$Root/src/cli")) {
  # fallback: assume we're launched from repo root
  $Root = (Get-Location).Path
}
$Cli = Join-Path $Root "src/cli"

$Targets = @{
  "command_registry.cpp" = @{
    Path = Join-Path $Cli "command_registry.cpp"
    Description = "Register alias: 'SET ORDER TO' -> SETORDER handler"
  }
  "cmd_help.cpp" = @{
    Path = Join-Path $Cli "cmd_help.cpp"
    Description = "Document INDEX / SET ORDER / SEEK canonical syntax"
  }
  "cmd_status.cpp" = @{
    Path = Join-Path $Cli "cmd_status.cpp"
    Description = "Banner text: 'Active tag: ...'"
  }
  "cmd_struct.cpp" = @{
    Path = Join-Path $Cli "cmd_struct.cpp"
    Description = "Banner text: 'Active tag: ...'"
  }
}

function Backup-Once {
  param([string]$Path)
  if (-not (Test-Path $Path)) { return }
  $bak = "$Path.bak"
  if (-not (Test-Path $bak)) {
    Copy-Item -LiteralPath $Path -Destination $bak
    Write-Host "  [+] Backup created: $bak" -ForegroundColor DarkGray
  }
}

function Edit-File {
  param(
    [string]$Path,
    [ScriptBlock]$Transform
  )
  if (-not (Test-Path $Path)) {
    Write-Warning "  [!] Missing file: $Path (skipping)"
    return $false
  }
  Backup-Once $Path
  $raw = Get-Content -LiteralPath $Path -Raw
  $out = & $Transform $raw
  if ($out -ne $raw) {
    Set-Content -LiteralPath $Path -Value $out -NoNewline
    Write-Host "  [~] Updated: $Path" -ForegroundColor Green
    return $true
  } else {
    Write-Host "  [=] No changes needed: $Path" -ForegroundColor DarkGray
    return $false
  }
}

Write-Host "=== Fix TAG logic: start ===" -ForegroundColor Cyan

# 1) command_registry.cpp — add alias "SET ORDER TO" -> SETORDER handler
Edit-File $Targets["command_registry.cpp"].Path {
  param($raw)

  # Try to find the registration line for SETORDER
  # Examples we try to match:
  #   registry.add("SETORDER", cmd_SETORDER);
  #   add_command("SETORDER", cmd_SETORDER);
  #   reg.add("SETORDER", &cmd_SETORDER);
  $pattern = '(?<prefix>^\s*[^;\n]*?add[^;\n]*?\(\s*")SETORDER(")\s*,\s*(?<handler>&?\s*cmd_[A-Za-z0-9_]+)\s*\)\s*;\s*$'
  $lines = $raw -split "`r?`n"
  $idx = -1
  $handler = $null
  for ($i=0; $i -lt $lines.Count; $i++) {
    $m = [regex]::Match($lines[$i], $pattern, 'IgnoreCase, Multiline')
    if ($m.Success) {
      $idx = $i
      $handler = $m.Groups['handler'].Value
      break
    }
  }

  if ($idx -ge 0 -and $handler) {
    # Check if alias already exists
    $already = $false
    for ($j=0; $j -lt $lines.Count; $j++) {
      if ($lines[$j] -match 'add[^;\n]*\(\s*"SET ORDER TO"\s*,') { $already = $true; break }
    }
    if (-not $already) {
      $prefix = [regex]::Match($lines[$idx], $pattern).Groups['prefix'].Value
      $insert = "$prefix" + 'SET ORDER TO"' + ", $handler);"  # keep handler & call style intact
      $indent = ($lines[$idx] -match '^\s*') ? ([regex]::Match($lines[$idx],'^\s*').Value) : ''
      $lines = $lines[0..$idx] + @("$indent$insert") + $lines[($idx+1)..($lines.Count-1)]
      return ($lines -join "`r`n")
    }
  }

  return $raw
}

# 2) cmd_help.cpp — ensure canonical examples are visible
Edit-File $Targets["cmd_help.cpp"].Path {
  param($raw)
  $addHelp = @"
  
  // --- normalized TAG/ORDER/SEEK help (injected) ---
  std::cout << "\nINDEX\n";
  std::cout << "  INDEX ON <expr> TAG <name> [ASC|DESC]\n";
  std::cout << "  e.g. INDEX ON LAST_NAME TAG LNAME\n";
  std::cout << "       INDEX ON LAST_NAME+FIRST_NAME TAG NAME\n";
  std::cout << "\nSET ORDER\n";
  std::cout << "  SET ORDER TO TAG <name>\n";
  std::cout << "  SET ORDER TO <n>\n";
  std::cout << "  SET ORDER TO 0 | NONE\n";
  std::cout << "\nSEEK\n";
  std::cout << "  SEEK <key>     (uses the currently selected TAG)\n";
  std::cout << "  e.g. SET ORDER TO TAG NAME  THEN  SEEK \"NguyenLinh\"\n";
  // --- end injection ---
"@
  # Insert just before the usual end of help; try to place after a HELP banner.
  if ($raw -notmatch 'normalized TAG/ORDER/SEEK help') {
    # Heuristic: append to end, safe and idempotent
    return ($raw.TrimEnd() + "`r`n" + $addHelp)
  }
  return $raw
}

# 3) cmd_status.cpp / cmd_struct.cpp — change "Active order:" banner to "Active tag:"
$bannerFind = 'Active\s+order:\s*TAG\s*\(none\)\s*\(ASCEND\)|Active\s+order:.*'
$bannerReplace = 'Active tag: (none)'

foreach ($k in @("cmd_status.cpp","cmd_struct.cpp")) {
  Edit-File $Targets[$k].Path {
    param($raw)
    $new = [regex]::Replace($raw, $bannerFind, $bannerReplace)
    return $new
  } | Out-Null
}

Write-Host "=== Fix TAG logic: file edits done ===" -ForegroundColor Cyan

# 4) Build (Release)
try {
  Push-Location $Root
  if (-not (Test-Path "build")) {
    cmake -S . -B build | Out-Host
  }
  cmake --build build --config Release | Out-Host
  Write-Host "=== Build finished ===" -ForegroundColor Green
} finally {
  Pop-Location
}

Write-Host ""
Write-Host "Next steps / Quick test:" -ForegroundColor Yellow
Write-Host "  1) Launch:   .\datarun.ps1"
Write-Host "  2) In DotTalk++:"
Write-Host "       USE STUDENTS"
Write-Host "       INDEX ON STUDENT_ID TAG ID"
Write-Host "       SET ORDER TO TAG ID"
Write-Host "       SEEK 100004"
Write-Host ""
Write-Host "If SEEK complains about 'No active order' or similar, double-check the alias got registered." -ForegroundColor DarkGray
