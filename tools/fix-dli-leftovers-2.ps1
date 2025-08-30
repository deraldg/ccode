Param(
  [switch]$Build
)

$ErrorActionPreference = 'Stop'
function Patch-File($path, [ScriptBlock]$transform) {
  if (!(Test-Path $path)) { return }
  $orig = Get-Content $path -Raw
  $new  = & $transform $orig
  if ($new -ne $orig) {
    $new | Set-Content $path -Encoding UTF8
    Write-Host "Patched $path"
  }
}

# 1) colors.hpp: move to dli::colors and add a compatibility bridge
Patch-File "src/cli/colors.hpp" {
  param($t)
  $t = $t -replace '(?s)namespace\s+cli::colors\s*\{','namespace dli::colors {'
  $t = $t -replace '(?s)\}\s*//\s*namespace\s+cli::colors','} // namespace dli::colors'
  # Ensure dli::colors declaration exists (idempotent)
  if ($t -notmatch 'namespace\s+dli::colors') {
    $t = @'
#pragma once
#include <string>
namespace dli { namespace colors {
  enum class Theme { Default, Green, Amber };
  Theme parseTheme(const std::string& s);
  void  applyTheme(Theme t);
}} // namespace dli::colors
// Back-compat shim
namespace cli { namespace colors {
  using Theme = dli::colors::Theme;
  inline Theme parseTheme(const std::string& s){ return dli::colors::parseTheme(s); }
  inline void  applyTheme(Theme t)             { dli::colors::applyTheme(t); }
}} // namespace cli::colors
'@
  }
  # Back-compat shim if missing
  if ($t -notmatch 'namespace\s+cli\s*\{\s*namespace\s+colors') {
    $shim = @'
// Back-compat shim
namespace cli { namespace colors {
  using Theme = dli::colors::Theme;
  inline Theme parseTheme(const std::string& s){ return dli::colors::parseTheme(s); }
  inline void  applyTheme(Theme t)             { dli::colors::applyTheme(t); }
}} // namespace cli::colors
'@
    $t = $t + "`r`n" + $shim
  }
  $t
}

# 2) colors.cpp: switch namespace body to dli::colors
Patch-File "src/cli/colors.cpp" {
  param($t)
  $t = $t -replace '(?s)namespace\s+cli::colors\s*\{','namespace dli::colors {'
  $t = $t -replace '(?s)\}\s*//\s*namespace\s+cli::colors','} // namespace dli::colors'
  $t
}

# 3) cmd_color.cpp: use dli::colors and ensure include is present
Patch-File "src/cli/cmd_color.cpp" {
  param($t)
  $t = $t -replace 'using\s+namespace\s+cli::colors\s*;','using namespace dli::colors;'
  if ($t -notmatch 'colors\.hpp') {
  $t = $t -replace '(\#include\s*".*")', "$1`r`n#include `"colors.hpp`""

  }
  $t
}

# 4) Replace cli::registry() with dli::registry() in specific files
$registryFiles = @(
  "src/cli/cmd_help_predicates.cpp",
  "src/cli/cmd_scan.cpp",
  "src/cli/cmd_setcase.cpp",
  "src/cli/shell_api.cpp",
  "src/cli/cmd_dotscript.cpp"
)
foreach ($f in $registryFiles) {
  Patch-File $f {
    param($t)
    $t -replace '\bcli::registry\b','dli::registry'
  }
}

# 5) shell_api.cpp: initialize const bool handled (MSVC complains otherwise)
Patch-File "src/cli/shell_api.cpp" {
  param($t)
  # If there's a lone 'const bool handled;' make it false
  $t = $t -replace '(?m)^\s*const\s+bool\s+handled\s*;\s*$', 'const bool handled = false;'
  $t
}

# 6) Quick sanity grep
Write-Host "`nScanning for leftovers..."
$left = git grep -n -E "\bcli::|namespace\s+cli::colors|using\s+namespace\s+cli::colors" 2>$null
if ($left) {
  Write-Host $left
} else {
  Write-Host "No obvious leftovers."
}

if ($Build) {
  Write-Host "`nBuilding..."
  cmake --build build --config Release --target dottalkpp
}
