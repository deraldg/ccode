# 5) Global but safe sweep across code files:
$codeFiles = git ls-files | Where-Object { $_ -match '\.(h|hh|hpp|ipp|hxx|c|cc|cpp|cxx)$' }

$skipped = 0
foreach ($f in $codeFiles) {
  $full = Join-Path $repoRoot $f

  if (!(Test-Path -LiteralPath $full)) {
    # Skip paths that aren’t present in the working tree (moved, sparse, etc.)
    Write-Host "Skip (not found): $f" -ForegroundColor DarkGray
    $skipped++
    continue
  }

  $orig = Get-Content -LiteralPath $full -Raw
  $patched = $orig

  # Replace cli:: with dli::
  $patched = [regex]::Replace($patched, '\bcli::', 'dli::')

  # Replace exact namespace cli (avoid matching words like client)
  $patched = [regex]::Replace($patched, '(\bnamespace\s+)cli(\b)', '${1}dli')

  if ($patched -ne $orig) {
    Copy-Item -LiteralPath $full -Destination $backupDir -Force
    Set-Content -LiteralPath $full -Value $patched -Encoding UTF8
    Write-Host "Swept $f" -ForegroundColor DarkYellow
  }
}

if ($skipped -gt 0) {
  Write-Host "Skipped $skipped file(s) that were listed by git but not present on disk." -ForegroundColor Yellow
}
