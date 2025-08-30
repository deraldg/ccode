# --- fix-cli-to-dli-leftovers.ps1 ---
# Runs from repo root. Updates namespaces/usages, then rebuilds.

$ErrorActionPreference = 'Stop'
$repo = Get-Location

# 1) Patch specific files that still reference cli:: or declare namespace cli
$touch = @(
  'src/cli/colors.hpp',
  'src/cli/colors.cpp',
  'src/cli/cmd_color.cpp',
  'src/cli/cmd_help_predicates.cpp',
  'src/cli/cmd_scan.cpp',
  'src/cli/cmd_setcase.cpp',
  'src/cli/shell_api.cpp',
  'src/cli/cmd_dotscript.cpp'
)

foreach ($f in $touch) {
  if (!(Test-Path $f)) { Write-Host "Skip (missing): $f" -ForegroundColor DarkGray; continue }
  $orig = Get-Content $f -Raw

  $patched = $orig
  # a) convert cli:: -> dli::
  $patched = [regex]::Replace($patched, '\bcli::', 'dli::')

  # b) convert 'namespace cli' -> 'namespace dli'
  $patched = [regex]::Replace($patched, '(\bnamespace\s+)cli(\b)', '${1}dli')

  if ($patched -ne $orig) {
    Set-Content $f -Value $patched -Encoding UTF8
    Write-Host "Patched $f" -ForegroundColor Yellow
  } else {
    Write-Host "No change $f" -ForegroundColor DarkGray
  }
}

# 2) Extra safety for colors headers: ensure expected symbols exist under dli::colors
# (idempotent, only tweaks if needed)
function Ensure-Contains {
  param($file, $pattern, $injectText)
  $txt = Get-Content $file -Raw
  if ($txt -notmatch $pattern) {
    $txt += "`r`n$injectText`r`n"
    Set-Content $file -Value $txt -Encoding UTF8
    Write-Host "Augmented $file with missing declarations" -ForegroundColor Yellow
  }
}

# Make sure colors.hpp has the declarations in dli::colors
if (Test-Path 'src/cli/colors.hpp') {
  Ensure-Contains 'src/cli/colors.hpp' '\bnamespace\s+dli\s*::\s*colors\b' @'
namespace dli { namespace colors {
  enum class Theme { Default, Green, Amber };
  Theme parseTheme(const std::string& s);
  void  applyTheme(Theme t);
} } // namespace dli::colors
'@
}

# 3) shell_api had a “const must be initialized” report at line ~64
# If there is “const bool handled;” make it a plain bool with a default value.
$api = 'src/cli/shell_api.cpp'
if (Test-Path $api) {
  $txt = Get-Content $api -Raw
  $txt2 = $txt -replace '(?m)^\s*const\s+bool\s+handled\s*;\s*$', 'bool handled = false;'
  if ($txt2 -ne $txt) {
    Set-Content $api -Value $txt2 -Encoding UTF8
    Write-Host "Initialized shell_api.cpp: handled = false" -ForegroundColor Yellow
  }
}

# 4) Quick grep to confirm no cli:: remains in real sources (excluding comments/tools)
Write-Host "`n--- Post-patch grep ---" -ForegroundColor Cyan
git grep -n -E "\bcli::" -- ':!tools/*' ':!*.md' ':!docs/*' ':!**/*.txt' || $true
git grep -n -E "^\s*namespace\s+cli\b" -- ':!tools/*' ':!*.md' ':!docs/*' ':!**/*.txt' || $true

# 5) Build
Write-Host "`n--- Building ---" -ForegroundColor Cyan
cmake --build build --config Release --target dottalkpp
