<#
  Help text tweak to reflect hardened APPEND and to ensure no mention of APPEND_BLANK.
  Usage (from repo root):
    powershell -ExecutionPolicy Bypass -File .\tools\apply_help_tweak.ps1
#>

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$HelpFile = Join-Path (Get-Location) "src\cli\cmd_help.cpp"
if (-not (Test-Path -LiteralPath $HelpFile)) {
  throw "Could not find src\cli\cmd_help.cpp"
}

$content = Get-Content -LiteralPath $HelpFile -Raw

# Replace any APPEND_BLANK usage line with the consolidated form
$content = $content -replace '(?m)^\s*<<\s*"APPEND_BLANK\s*\[n\].*$', '        << "APPEND [BLANK|-B] [n]            # append n blank records (default 1)\n"'
# Ensure APPEND usage line exists (insert after REPLACE line if missing)
if ($content -notmatch 'APPEND \[BLANK\|\-B\] \[n\]') {
  $content = $content -replace '(?m)(^\s*<<\s*"REPLACE.*\n)', "`$1        << \"APPEND [BLANK|-B] [n]            # append n blank records (default 1)\n\""
}

# Remove APPEND_BLANK token from the commands list
$content = ($content -split "`n") | ForEach-Object {
  if ($_ -match 'APPEND_BLANK') { $_ -replace 'APPEND_BLANK\s*', '' } else { $_ }
} | Out-String

Set-Content -LiteralPath $HelpFile -Value $content -Encoding UTF8
Write-Host "Help text updated." -ForegroundColor Green
