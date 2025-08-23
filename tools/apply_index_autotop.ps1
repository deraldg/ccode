<#
  Inject auto-TOP call after successful INDEX creation.
  - Adds:   #include "order_hooks.hpp"
  - After the line that prints "Index written:" it inserts:  order_auto_top(db);
  Usage (from repo root):
    powershell -ExecutionPolicy Bypass -File .\tools\apply_index_autotop.ps1
#>

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$File = Join-Path (Get-Location) "src\cli\cmd_index.cpp"
if (-not (Test-Path -LiteralPath $File)) {
  throw "Could not find src\cli\cmd_index.cpp"
}

$src = Get-Content -LiteralPath $File -Raw

# 1) Ensure the include exists once
if ($src -notmatch '(?m)^\s*#\s*include\s+"order_hooks\.hpp"\s*$') {
  $src = $src -replace '(?s)(#\s*include\s+"[^"]+"\s*\n)(?!.*#\s*include\s+"[^"]+"\s*\n)', "`$1#include `"order_hooks.hpp`"`n"
}

# 2) Insert order_auto_top(db); after a line that prints "Index written:"
$pattern = '(?m)^(.*"Index written:.*"\s*;.*)$'
if ($src -match $pattern) {
  $src = $src -replace $pattern, "`$1`n    order_auto_top(db);"
} else {
  Write-Warning "Couldn't locate the 'Index written:' line. No auto-top injected."
}

Set-Content -LiteralPath $File -Value $src -Encoding UTF8
Write-Host "Auto-TOP injection complete (or safely skipped)." -ForegroundColor Green
