param(
  # path to exe (default to build\Release)
  [string]$Exe = (Join-Path $PSScriptRoot '..\build\Release\dottalkpp.exe'),

  # data dir where DBFs live (tests run with this as working dir so scripts can do: USE students)
  [string]$DataDir = (Join-Path $PSScriptRoot '..\data'),

  # directory containing .dt scripts
  [string]$TestsDir = (Join-Path $PSScriptRoot '.'),

  # where to write raw, filtered, and diffs
  [string]$ResultsDir = (Join-Path $PSScriptRoot '..\results')
)

$ErrorActionPreference = 'Stop'

function New-Dir([string]$p) { if (-not (Test-Path $p)) { New-Item -ItemType Directory -Path $p | Out-Null } }

# --- sanity checks
if (-not (Test-Path $Exe))     { throw "EXE not found: $Exe" }
if (-not (Test-Path $DataDir)) { throw "Data dir not found: $DataDir" }
if (-not (Test-Path $TestsDir)){ throw "Tests dir not found: $TestsDir" }

New-Dir $ResultsDir

# simple output normalizer to make comparisons stable
function Normalize-Text([string]$text) {
  $lines = $text -split "`r?`n"

  $lines = $lines |
    Where-Object { $_ -notmatch '^\[wire\]\s' } |                 # drop wire pointers
    ForEach-Object {
      $_ -replace '(?i)\bC:\\Users\\[^\\]+\\code\\ccode','<ROOT>' # scrub absolute root
         -replace '\b0x[0-9A-Fa-f]+','<PTR>'                      # hex ptrs if any
         -replace '\s+$',''                                       # trim right
    }

  # collapse repeated blank lines
  ($lines -join "`n") -replace "(\r?\n){3,}","`n`n"
}

# run one test: feeds script to exe, captures, normalizes, compares to .golden if present
function Invoke-DotTalkTest([string]$scriptPath) {
  $name = [IO.Path]::GetFileNameWithoutExtension($scriptPath)
  $rawOut = Join-Path $ResultsDir "$name.out.txt"
  $rawErr = Join-Path $ResultsDir "$name.err.txt"
  $filOut = Join-Path $ResultsDir "$name.norm.txt"
  $golden = Join-Path $TestsDir "expected\$name.golden.txt"
  $diff   = Join-Path $ResultsDir "$name.diff.txt"

  Push-Location $DataDir
  try {
    # run with stdin redirected from script; working dir = data
    # note: use Start-Process to keep file redirection reliable on Windows
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $Exe
    $psi.RedirectStandardInput  = $true
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError  = $true
    $psi.UseShellExecute = $false
    $p = New-Object System.Diagnostics.Process
    $p.StartInfo = $psi
    [void]$p.Start()

    # feed the script
    Get-Content -Raw -LiteralPath $scriptPath | % { $p.StandardInput.Write($_) }
    $p.StandardInput.Close()

    # collect outputs
    $stdout = $p.StandardOutput.ReadToEnd()
    $stderr = $p.StandardError.ReadToEnd()
    $p.WaitForExit()

    # write raw
    Set-Content -LiteralPath $rawOut -Value $stdout -Encoding UTF8
    if ($stderr) { Set-Content -LiteralPath $rawErr -Value $stderr -Encoding UTF8 } else { '' | Set-Content $rawErr }

    # normalize
    $norm = Normalize-Text $stdout
    Set-Content -LiteralPath $filOut -Value $norm -Encoding UTF8

    # compare if golden exists
    if (Test-Path $golden) {
      $gold = Get-Content -Raw -LiteralPath $golden
      $gold = Normalize-Text $gold
      if ($gold -ne $norm) {
        # simple unified diff-ish
        $left  = $gold -split "`r?`n"
        $right = $norm -split "`r?`n"
        $max = [Math]::Max($left.Count, $right.Count)
        $sb = New-Object System.Text.StringBuilder
        for ($i=0; $i -lt $max; $i++) {
          $l = if ($i -lt $left.Count)  { $left[$i] }  else { "<eof>" }
          $r = if ($i -lt $right.Count) { $right[$i] } else { "<eof>" }
          if ($l -ne $r) { [void]$sb.AppendLine(("{0,4}: - {1}`n{0,4}: + {2}" -f ($i+1), $l, $r)) }
        }
        Set-Content -LiteralPath $diff -Value $sb.ToString() -Encoding UTF8
        return [PSCustomObject]@{ Name=$name; Pass=$false; Out=$rawOut; Norm=$filOut; Err=$rawErr; Diff=$diff }
      } else {
        if (Test-Path $diff) { Remove-Item -Force $diff }
        return [PSCustomObject]@{ Name=$name; Pass=$true;  Out=$rawOut; Norm=$filOut; Err=$rawErr; Diff=$null }
      }
    } else {
      # no golden → treat as smoke run only
      return [PSCustomObject]@{ Name=$name; Pass=$true; Out=$rawOut; Norm=$filOut; Err=$rawErr; Diff=$null }
    }
  }
  finally { Pop-Location }
}

# discover tests (*.dt) and run
$tests = Get-ChildItem -LiteralPath $TestsDir -Filter '*.dt' | Sort-Object Name
if ($tests.Count -eq 0) {
  Write-Host "No .dt script files found in $TestsDir"
  exit 1
}

$result = @()
foreach ($t in $tests) {
  Write-Host ("[ RUN ] {0}" -f $t.Name)
  $r = Invoke-DotTalkTest $t.FullName
  $status = if ($r.Pass) { "PASS" } else { "FAIL" }
  Write-Host ("[ {0} ] {1}" -f $status, $r.Name)
  if (-not $r.Pass) { Write-Host ("       see: {0}" -f $r.Diff) }
  $result += $r
}

# summary
$pass = ($result | Where-Object { $_.Pass }).Count
$fail = $result.Count - $pass
Write-Host ""
Write-Host ("Summary: {0} passed, {1} failed, {2} total" -f $pass,$fail,$result.Count)
if ($fail -gt 0) { exit 2 } else { exit 0 }
