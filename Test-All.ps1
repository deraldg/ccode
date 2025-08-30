param(
  [string]$Exe        = "build\Release\dottalkpp.exe",
  [string]$TestsDir   = "tests",
  [string]$DataDir    = "data",
  [string]$ResultsDir = "results",
  [switch]$Normalize  = $true,
  [switch]$Diff       = $true,
  [switch]$UpdateGolden # if set, copy normalized -> golden when no golden exists
)

$RawDir   = Join-Path $ResultsDir "raw"
$NormDir  = Join-Path $ResultsDir "normalized"
$DiffDir  = Join-Path $ResultsDir "diff"
$GoldenDir= Join-Path $ResultsDir "golden"

New-Item -ItemType Directory -ea 0 -Path $ResultsDir,$RawDir,$NormDir,$DiffDir,$GoldenDir | Out-Null

if (!(Test-Path $Exe)) { Write-Error "Missing exe: $Exe"; exit 2 }
if (!(Test-Path $TestsDir)) { Write-Error "Missing tests dir: $TestsDir"; exit 2 }
if (!(Test-Path $DataDir)) { Write-Error "Missing data dir: $DataDir"; exit 2 }

function Normalize-Content([string]$content) {
  $lines = $content -split "\r?\n"

  $lines = $lines |
    Where-Object {
      $_ -notmatch '^\[wire\]\s' -and             # drop [wire] diagnostics
      $_ -notmatch '^DotTalk\+\+ '                # drop banner
    } |
    ForEach-Object {
      # Optionally strip volatile spacing; keep prompts/echoes
      $_ -replace '\s+$',''
    }

  ($lines -join "`r`n").Trim() + "`r`n"
}

$all = Get-ChildItem -Path $TestsDir -File -Filter *.txt | Sort-Object Name
if (-not $all) { Write-Error "No tests found in $TestsDir"; exit 3 }

$fail = 0
$ran = 0

foreach ($t in $all) {
  $name = [IO.Path]::GetFileNameWithoutExtension($t.Name)
  $rawOut = Join-Path $RawDir  ($name + ".out")
  $rawErr = Join-Path $RawDir  ($name + ".err")
  $norOut = Join-Path $NormDir ($name + ".out")
  $difOut = Join-Path $DiffDir ($name + ".diff")
  $golden = Join-Path $GoldenDir ($name + ".out")

  Write-Host "==> $name"

  Push-Location $DataDir
  try {
    # Feed script to stdin, capture out/err to files in results\raw
    # Use PowerShell’s redirection so prompts don’t block.
    # NOTE: we pass absolute paths to avoid confusion.
    $scriptPath = Resolve-Path $t.FullName
    $exePath    = Resolve-Path (Join-Path $PWD "..\$Exe")

    # Invoke with redirection in one shell (>& for stderr, > for stdout)
    # Using cmd /c to preserve input redirection behavior.
    $cmd = "cmd /c `"$exePath`" < `"$scriptPath`" 1> `"$rawOut`" 2> `"$rawErr`""
    $p = Start-Process -FilePath "cmd.exe" -ArgumentList "/c", $cmd -NoNewWindow -Wait -PassThru
    $code = $p.ExitCode
  }
  finally { Pop-Location }

  $ran++

  if ($Normalize) {
    $raw = if (Test-Path $rawOut) { Get-Content $rawOut -Raw } else { "" }
    $norm = Normalize-Content $raw
    Set-Content -Path $norOut -Value $norm -NoNewline
  }

  if ($Diff) {
    if (Test-Path $golden) {
      $expected = Get-Content $golden -Raw
      $actual   = Get-Content $norOut -Raw

      if ($expected -ne $actual) {
        # Simple line diff
        $expLines = $expected -split "\r?\n"
        $actLines = $actual   -split "\r?\n"
        $max = [Math]::Max($expLines.Length, $actLines.Length)
        $sb = New-Object System.Text.StringBuilder
        for ($i=0; $i -lt $max; $i++) {
          $e = if ($i -lt $expLines.Length) { $expLines[$i] } else { "" }
          $a = if ($i -lt $actLines.Length) { $actLines[$i] } else { "" }
          if ($e -ne $a) {
            [void]$sb.AppendLine(("LINE {0:D4}" -f ($i+1)))
            [void]$sb.AppendLine("  EXP: " + $e)
            [void]$sb.AppendLine("  ACT: " + $a)
          }
        }
        Set-Content -Path $difOut -Value $sb.ToString()
        Write-Warning "DIFF: $name"
        $fail++
      } else {
        if (Test-Path $difOut) { Remove-Item $difOut -Force }
      }
    }
    elseif ($UpdateGolden) {
      Copy-Item $norOut $golden
      Write-Host "  (golden created)"
    }
    else {
      Write-Host "  (no golden found; run with -UpdateGolden to create)"
    }
  }
}

Write-Host "`n=== SUMMARY ==="
Write-Host ("Ran: {0}   Failed(diffs): {1}" -f $ran, $fail)

if ($fail -gt 0) { exit 1 } else { exit 0 }
