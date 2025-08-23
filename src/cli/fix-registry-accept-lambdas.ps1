param(
  [string]$Root = "."
)

$ErrorActionPreference = "Stop"
$rootPath = (Resolve-Path $Root).Path
Write-Host "Repo root: $rootPath"

# 1) Remove duplicate header in src\cli if present
$dupHeader = Join-Path $rootPath "src\cli\command_registry.hpp"
if (Test-Path $dupHeader) {
  Write-Host "Removing duplicate header: $dupHeader"
  Remove-Item $dupHeader -Force
} else {
  Write-Host "No duplicate header in src\cli."
}

# 2) Patch include\command_registry.hpp to accept std::function handlers
$hdr = Join-Path $rootPath "include\command_registry.hpp"
if (!(Test-Path $hdr)) {
  throw "Header not found: $hdr"
}
$txt = Get-Content $hdr -Raw
$orig = $txt

# Ensure <functional> is included
if ($txt -notmatch '(?m)^\s*#\s*include\s*<functional>\s*$') {
  # Add after the last #include line near the top
  if ($txt -match '(?s)^(.*?#include[^\r\n]+(?:\r?\n))+') {
    $txt = $txt -replace '(?m)(^(\s*#\s*include[^\r\n]+\r?\n)+)',
      ('$1#include <functional>' + [Environment]::NewLine)
    Write-Host "Inserted #include <functional>."
  } else {
    $txt = "#include <functional>`r`n$txt"
    Write-Host "Prepended #include <functional>."
  }
}

# Replace typedef/using of Handler to std::function
# Common patterns to replace:
#   using Handler = void(*)(...);
#   typedef void (*Handler)(...);
$txt = $txt -replace 'using\s+Handler\s*=\s*void\s*\(\s*\*\s*\)\s*\(\s*([^\)]*?)\s*\)\s*;',
                     'using Handler = std::function<void($1)>;'
$txt = $txt -replace 'typedef\s+void\s*\(\s*\*\s*Handler\s*\)\s*\(\s*([^\)]*?)\s*\)\s*;',
                     'using Handler = std::function<void($1)>;'

# Optional: if Handler is declared in a helper namespace (e.g. command_registry::Handler),
# the text above still works because we keep the name "Handler" the same.

if ($txt -ne $orig) {
  Copy-Item $hdr "$hdr.bak" -Force
  Set-Content -Path $hdr -Value $txt -NoNewline
  Write-Host "Patched $hdr. Backup: $hdr.bak"
} else {
  Write-Host "No Handler typedef/using changes detected in $hdr (already ok?)."
}

# 3) Show any remaining lambda-style registrations in shell.cpp (for your awareness)
$shell = Join-Path $rootPath "src\cli\shell.cpp"
if (Test-Path $shell) {
  Write-Host "`nLambda registrations still present (if any):"
  Select-String -Path $shell -Pattern 'reg\.add\([^,]+,\s*\[' -Context 0,1 -AllMatches | ForEach-Object {
    "$($_.Filename):$($_.LineNumber): $($_.Line.Trim())"
  }
} else {
  Write-Host "No shell.cpp found at $shell (skipping scan)."
}

# 4) Clean rebuild
$build = Join-Path $rootPath "build"
if (Test-Path $build) {
  Write-Host "Removing build folder to avoid stale headers/objs..."
  Remove-Item -Recurse -Force $build
}

Write-Host "`nReconfiguring with CMake..."
cmake -S $rootPath -B $build

Write-Host "Building target dottalkpp (Release)..."
cmake --build $build --config Release --target dottalkpp
