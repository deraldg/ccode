# fix-reg-placement.ps1
Set-StrictMode -Version Latest
$hdr = "include\command_registry.hpp"
$cpp = "src\cli\command_registry.cpp"

if (-not (Test-Path $hdr)) { throw "Missing $hdr" }
if (-not (Test-Path $cpp)) { throw "Missing $cpp" }

Copy-Item $hdr "$hdr.bak_$(Get-Date -Format yyyyMMddHHmmss)"
Copy-Item $cpp "$cpp.bak_$(Get-Date -Format yyyyMMddHHmmss)"

# --- Header: ensure only an extern, no inline/global definition ---
$h = Get-Content $hdr -Raw

# Remove any prior 'inline CommandRegistry reg{};' we might have added
$h = $h -replace '(?s)namespace\s+cli\s*\{\s*inline\s+CommandRegistry\s+reg\s*\{\s*\}\s*;\s*\}', ''

# Remove duplicate externs to avoid re-adding multiple times
$h = $h -replace '(?s)namespace\s+cli\s*\{\s*extern\s+CommandRegistry\s+reg\s*;\s*\}', ''

# Append a single extern at the end
$h = $h.TrimEnd() + "`r`n`r`nnamespace cli { extern CommandRegistry reg; }`r`n"
Set-Content $hdr $h -Encoding UTF8

# --- .cpp: ensure includes first, then ONE definition of reg ---
$c = Get-Content $cpp -Raw

# Ensure the header is included
if ($c -notmatch '(?m)^\s*#\s*include\s*"command_registry\.hpp"') {
  $c = "#include `"command_registry.hpp`"`r`n" + $c
}

# Remove any existing definitions of 'CommandRegistry reg'
$c = $c -replace '(?s)namespace\s+cli\s*\{\s*CommandRegistry\s+reg\s*(\{\s*\})?\s*;\s*\}', ''

# Find the last #include line and insert the definition right after
$lines = $c -split "`r`n"
$lastInc = ($lines | Select-String -Pattern '^\s*#\s*include\b' -AllMatches | Select-Object -Last 1)
$idx = if ($lastInc) { $lastInc.LineNumber } else { 0 }

$def = @"
namespace cli {
    CommandRegistry reg{};
}
"@.TrimEnd()

if ($idx -gt 0) {
  $before = ($lines[0..($idx-1)] -join "`r`n")
  $after  = ($lines[$idx..($lines.Length-1)] -join "`r`n")
  $c = $before + "`r`n`r`n" + $def + "`r`n`r`n" + $after
} else {
  $c = $def + "`r`n`r`n" + $c
}

Set-Content $cpp $c -Encoding UTF8

# Reconfigure & build
if (Test-Path build) { Remove-Item -Recurse -Force build }
cmake -S . -B build
cmake --build build --config Release --target dottalkpp
