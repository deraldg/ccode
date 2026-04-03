Param(
  [string]$ExePath,
  [string]$BaselineDir = ".\baseline",
  [string]$LogsDir = ".\logs"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Push-Location -LiteralPath $PSScriptRoot
try {
  # Locate dottalkpp.exe
  $candidates = @()
  if ($ExePath) { $candidates += $ExePath }
  $candidates += @(
    ".\build\src\Release\dottalkpp.exe",
    ".\build\src\Debug\dottalkpp.exe",
    ".\build\Release\dottalkpp.exe",
    ".\build\Debug\dottalkpp.exe"
  )
  $Exe = $null
  foreach ($c in $candidates) {
    $full = Resolve-Path -LiteralPath $c -ErrorAction SilentlyContinue
    if ($full) { $Exe = $full.ProviderPath; break }
  }
  if (-not $Exe) {
    Write-Host "Tried:" -ForegroundColor Yellow
    $candidates | ForEach-Object { Write-Host "  $_" -ForegroundColor Yellow }
    throw "EXE not found. Pass -ExePath or build dottalkpp.exe"
  }
  Write-Host "Using EXE: $Exe" -ForegroundColor Cyan
  Write-Host "Working Dir: $PWD" -ForegroundColor Cyan

  # Ensure dirs
  if (!(Test-Path $LogsDir)) { New-Item -ItemType Directory -Force -Path $LogsDir | Out-Null }
  if (!(Test-Path $BaselineDir)) { New-Item -ItemType Directory -Force -Path $BaselineDir | Out-Null }

  # Test list
  $tests = @(
    'struct_inx_active.dts',
    'struct_cnx_tags.dts',
    'list_order_flip.dts',
    'full_baseline.dts',
    'inxi_attach_switching.dts',
    'cnx_tag_keyword_form.dts',
    'banner_consistency.dts'
  )

  $failures = @()
  foreach ($t in $tests) {
    $log = Join-Path $LogsDir ("{0}.log" -f ([IO.Path]::GetFileNameWithoutExtension($t)))
    Write-Host "`n=== Running $t ===" -ForegroundColor Green

    # Execute TEST command (program expects a single command-line string)
    & $Exe ("TEST {0} {1} VERBOSE" -f $t, $log)
    if ($LASTEXITCODE -ne 0) {
      $failures += "TEST failed: $t (exit $LASTEXITCODE)"
      continue
    }

    $baseline = Join-Path $BaselineDir (Split-Path $log -Leaf)
    if (Test-Path $baseline) {
      # Use fc for stable, numbered diff; write diff next to log.
      $diffFile = "$log.diff"
      cmd /c "fc /n `"$log`" `"$baseline`" > `"$diffFile`"" | Out-Null
      $diffExit = $LASTEXITCODE
      if ($diffExit -ne 0) {
        $failures += "DIFF: $t (see $diffFile)"
        Write-Warning "Difference detected for $t"
      } else {
        Write-Host "OK: $t matches baseline." -ForegroundColor DarkGreen
        if (Test-Path $diffFile) { Remove-Item $diffFile -Force }
      }
    } else {
      Write-Warning "No baseline for $t; seeding baseline."
      Copy-Item -LiteralPath $log -Destination $baseline -Force
    }
  }

  if ($failures.Count -gt 0) {
    Write-Host "`nFailures:" -ForegroundColor Red
    $failures | ForEach-Object { Write-Host " - $_" -ForegroundColor Red }
    exit 1
  }

  Write-Host "`nAll tests passed and match baseline." -ForegroundColor Green
  exit 0
}
finally {
  Pop-Location
}
