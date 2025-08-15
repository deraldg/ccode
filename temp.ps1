# scripts\fix_min.ps1
$ErrorActionPreference = 'Stop'
$root = '.'
$nl   = [Environment]::NewLine

# --- 1) Write canonical key_common.hpp ---------------------------------------
$kcPath = Join-Path $root 'include\xindex\key_common.hpp'
New-Item -ItemType Directory -Force -Path (Split-Path $kcPath) | Out-Null
$kc = @'
#pragma once
#include <cstdint>
#include <vector>

namespace xindex {

using Key   = std::vector<std::uint8_t>;
using RecNo = std::uint32_t;

struct KeyLess {
    bool operator()(const Key& a, const Key& b) const noexcept {
        return a < b; // vector comparison is lexicographic
    }
};

} // namespace xindex
'@
Set-Content -LiteralPath $kcPath -Value $kc -Encoding UTF8
Write-Host "✓ key_common.hpp written"

# --- 2) Patch headers under include\xindex -----------------------------------
$hdrDir = Join-Path $root 'include\xindex'
if (Test-Path $hdrDir) {
  Get-ChildItem -Recurse -LiteralPath $hdrDir -Filter *.hpp | ForEach-Object {
    $p = $_.FullName
    $c = Get-Content -LiteralPath $p -Raw

    # ensure #pragma once
    if ($c -notmatch '^\s*#pragma\s+once') { $c = '#pragma once' + $nl + $c }

    # ensure includes
    if ($c -notmatch '#include\s+"xindex/key_common\.hpp"') { $c = $c -replace '^\s*#pragma\s+once.*', '$0' + $nl + '#include "xindex/key_common.hpp"' }
    if ($_.Name -match '^bpt(_|ree)_backend\.hpp$') {
      if ($c -notmatch '#include\s+"xindex/index_backend\.hpp"') { $c = $c -replace '^\s*#pragma\s+once.*', '$0' + $nl + '#include "xindex/index_backend.hpp"' }
      if ($_.Name -match '^bptree_backend\.hpp$') {
        if ($c -notmatch '#include\s+<iosfwd>')   { $c = $c -replace '^\s*#pragma\s+once.*', '$0' + $nl + '#include <iosfwd>' }
      }
      if ($c -notmatch '#include\s+<map>')       { $c = $c -replace '^\s*#pragma\s+once.*', '$0' + $nl + '#include <map>' }
      if ($c -notmatch '#include\s+<memory>')    { $c = $c -replace '^\s*#pragma\s+once.*', '$0' + $nl + '#include <memory>' }
      if ($c -notmatch '#include\s+<optional>')  { $c = $c -replace '^\s*#pragma\s+once.*', '$0' + $nl + '#include <optional>' }
      if ($c -notmatch '#include\s+<string>')    { $c = $c -replace '^\s*#pragma\s+once.*', '$0' + $nl + '#include <string>' }
    }

    # drop any forward-decl of KeyLess and any duplicate definition
    $c = ($c -split "`r?`n" | Where-Object { $_ -notmatch '^\s*struct\s+KeyLess\s*;\s*$' }) -join $nl
    $c = [regex]::Replace($c, 'struct\s+KeyLess\s*\{[\s\S]*?\};', '/* duplicate KeyLess removed (lives in key_common.hpp) */', [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)

    # remove duplicate using Key/RecNo here (keep only in key_common.hpp)
    if ($_.Name -ne 'key_common.hpp') {
      $c = ($c -split "`r?`n" | Where-Object { $_ -notmatch '^\s*using\s+(Key|RecNo)\s*=' }) -join $nl
    }

    # if someone accidentally declared ofstream/ifstream in a header, strip it
    $c = ($c -split "`r?`n" | Where-Object { $_ -notmatch '^\s*(std::)?(i|o)fstream\s+\w+\s*\(.*\)\s*;?\s*$' }) -join $nl

    # de-duplicate write_u32/read_u32 prototypes in bptree_backend.hpp (keep first)
    if ($_.Name -eq 'bptree_backend.hpp') {
      $script:seenW = $false
      $script:seenR = $false
      $c = [regex]::Replace($c, '(?m)^\s*(?:static\s+)?void\s+write_u32\s*\([^)]*\)\s*;\s*$', {
        param($m) if($script:seenW){''} else {$script:seenW=$true; $m.Value}
      })
      $c = [regex]::Replace($c, '(?m)^\s*(?:static\s+)?(?:std::)?(?:uint32_t|uint32_t)\s+read_u32\s*\([^)]*\)\s*;\s*$', {
        param($m) if($script:seenR){''} else {$script:seenR=$true; $m.Value}
      })
    }

    Set-Content -LiteralPath $p -Value $c -Encoding UTF8
    Write-Host "✓ header patched: $($p.Substring($root.Length).TrimStart('\','/'))"
  }
}

# --- 3) Patch .cpp sources for IO + key_common + std::ios::binary ----------
$srcDir = Join-Path $root 'src\xindex'
if (Test-Path $srcDir) {
  Get-ChildItem -Recurse -LiteralPath $srcDir -Filter *.cpp | ForEach-Object {
    $p = $_.FullName
    $c = Get-Content -LiteralPath $p -Raw

    # ensure includes (use $nl to avoid backtick-escape issues)
    if ($c -notmatch '#include\s+"xindex/key_common\.hpp"') { $c = '#include "xindex/key_common.hpp"' + $nl + $c }
    if ($c -notmatch '#include\s+<fstream>') { $c = '#include <fstream>' + $nl + $c }
    if ($c -notmatch '#include\s+<ios>')     { $c = '#include <ios>'     + $nl + $c }
    if ($c -match '\boptional<') {
      if ($c -notmatch '#include\s+<optional>') { $c = '#include <optional>' + $nl + $c }
    }

    # qualify binary flag
    $c = [regex]::Replace($c, '(^|[^A-Za-z0-9_])binary([^A-Za-z0-9_]|$)', '${1}std::ios::binary${2}')

    Set-Content -LiteralPath $p -Value $c -Encoding UTF8
    Write-Host "✓ source patched: $($p.Substring($root.Length).TrimStart('\','/'))"
  }
}

Write-Host "All fixes applied. Now rebuild."
