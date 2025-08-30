# fix-append-blank.ps1
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$root = $PSScriptRoot
$cpp  = Join-Path $root 'src\cli\cmd_append.cpp'
if (!(Test-Path $cpp)) { throw "Missing $cpp" }

# Read original
$orig  = Get-Content $cpp -Raw

# Strip any accidentally appended PowerShell/script lines and any previous wrapper attempts
$clean = $orig `
  -replace '(?m)^\s*#\s*Path.*\r?\n','' `
  -replace '(?m)^\s*Set-StrictMode.*\r?\n','' `
  -replace '(?m)^\s*\$ErrorActionPreference.*\r?\n','' `
  -replace '(?m)^\s*cmake\s.*\r?\n','' `
  -replace '(?m)^\s*Push-Location.*\r?\n','' `
  -replace '(?m)^\s*Pop-Location.*\r?\n','' `
  -replace '(?s)/\*\s*>>> APPEND_BLANK WRAPPER BEGIN >>>\s*\*/.*?/\*\s*<<< APPEND_BLANK WRAPPER END <<<\s*\*/','' `
  -replace '(?s)//\s*--- added by patch-append-blank\.ps1 ---.*','' `
  -replace '(?s)^\s*void\s+cmd_APPEND_BLANK\s*\([^)]*\)\s*\{.*?\}\s*',''

# The correct wrapper block (BOM-less UTF-8)
$wrapper = @'
/* >>> APPEND_BLANK WRAPPER BEGIN >>> */
#include <sstream>
namespace xbase { class DbArea; }
void cmd_APPEND(xbase::DbArea&, std::istringstream&);
void cmd_APPEND_BLANK(xbase::DbArea& A, std::istringstream& S) {
    // Forward to APPEND; the shell already put "BLANK" in the stream
    cmd_APPEND(A, S);
}
/* <<< APPEND_BLANK WRAPPER END <<< */
'@

# Ensure newline before appending
if ($clean -notmatch "(\r?\n)$") { $clean += "`r`n" }

# Backup & write
$backup = "$cpp.bak_$(Get-Date -Format 'yyyyMMddHHmmss')"
[IO.File]::WriteAllText($backup, $orig,  [Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllText($cpp,   $clean + $wrapper, [Text.UTF8Encoding]::new($false))
Write-Host "Patched $cpp (backup: $(Split-Path $backup -Leaf))"

# Rebuild from a clean build dir
$build = Join-Path $root 'build'
if (Test-Path $build) { Remove-Item -Recurse -Force $build }
cmake -S $root -B $build
cmake --build $build --config Release --target dottalkpp
