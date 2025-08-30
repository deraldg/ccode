# Parser Debug Zip – Scripts for ccode/DotTalk++

Below are two ready-to-use PowerShell scripts:

* **Make-ParserDebugZip.ps1** — full-featured bundle (sources + configs + optional build artifacts), with `-Verbose` and `-NoZip` options.
* **Make-ParserDebugZip-Fast.ps1** — ultra-fast bundle of **git-tracked** files using `git archive`.

> Save each block to the indicated filename in your repo root (e.g., `C:\Users\deral\code\ccode\`).

---

## Make-ParserDebugZip.ps1

```powershell
<#
  Make-ParserDebugZip.ps1
  Collect a reproducible "parser debug" bundle for DotTalk++/ccode.
  Run from the repo root (e.g., C:\Users\deral\code\ccode).

  Usage examples:
    .\Make-ParserDebugZip.ps1                          # default run
    .\Make-ParserDebugZip.ps1 -Verbose                 # show progress
    .\Make-ParserDebugZip.ps1 -IncludeBuild            # also include build\ binaries (slower/bigger)
    .\Make-ParserDebugZip.ps1 -NoZip                   # copy to staging only (no compression)
    powershell -NoProfile -ExecutionPolicy Bypass -File .\Make-ParserDebugZip.ps1 -Verbose
#>

[CmdletBinding()]
param(
  [string]$OutDir = ".\releases\parser-debug",
  [switch]$IncludeBuild,
  [switch]$NoZip
)

$ErrorActionPreference = "Stop"

function MkDirSafe($p) { if (-not (Test-Path -LiteralPath $p)) { New-Item -ItemType Directory -Path $p | Out-Null } }

# Roots to gather (present-only)
$roots = @(
  ".",             # ccode root
  "..\pcode",     # sibling if present
  "..\xcode"      # overlay if present
) | Where-Object { Test-Path -LiteralPath $_ }

MkDirSafe $OutDir
$stamp   = Get-Date -Format "yyyyMMdd-HHmmss"
$zipName = "parser-debug-$stamp.zip"
$zipPath = Join-Path $OutDir $zipName
$stage   = Join-Path $OutDir ("staging-" + $stamp)
MkDirSafe $stage

Write-Verbose "Output dir: $OutDir"
Write-Verbose "Staging dir: $stage"

# Patterns
$IncludePatterns = @(
  "**\\*.c","**\\*.cpp","**\\*.hpp","**\\*.h","**\\*.inl",
  "**\\CMakeLists.txt","**\\*.cmake",
  "**\\*.ps1","**\\*.bat","**\\*.sh",
  "**\\*.md","**\\*.txt"
)

$ConfigPicks = @(
  "build\\CMakeCache.txt",
  "build\\compile_commands.json",
  "build\\**\\*.vcxproj",
  "build\\**\\*.sln",
  "build\\**\\CMakeFiles\\**\\*.txt",
  "build\\**\\*.log"
)

$BinaryPicks = @(
  "build\\**\\*.exe",
  "build\\**\\*.lib",
  "build\\**\\*.pdb"
)

$UserLogPicks = @(
  ".\\*.log",
  "logs\\**\\*.log",
  "releases\\**\\*.manifest.txt",
  "releases\\**\\CHANGELOG*.md",
  "releases\\**\\*ReleaseNotes*.docx"
)

$ExcludeDirRegex = @(
  '\\\.git($|\\)', '\\\.vs($|\\)', '\\\.vscode($|\\)',
  'build($|\\)', '_ccode_build($|\\)', '_pcode_build($|\\)',
  'x64($|\\)','x86($|\\)','Debug($|\\)','Release($|\\)',
  'bin($|\\)','obj($|\\)','out($|\\)','ipch($|\\)',
  'CMakeFiles($|\\)', '\\\.cache($|\\)'
)

function Should-Exclude([string]$fullPath) {
  foreach ($rx in $ExcludeDirRegex) { if ($fullPath -match $rx) { return $true } }
  return $false
}

function Copy-Matches($root, $patterns) {
  foreach ($pat in $patterns) {
    Write-Verbose "Scanning '$root' for '$pat'"
    Get-ChildItem -LiteralPath $root -Recurse -File -Include $pat -ErrorAction SilentlyContinue |
      Where-Object { -not (Should-Exclude $_.FullName) } |
      ForEach-Object {
        $rootAbs = (Resolve-Path -LiteralPath $root).Path
        $rel = $_.FullName.Substring($rootAbs.Length).TrimStart('\\','/')
        $dest = Join-Path $stage (Join-Path ([IO.Path]::GetFileName($root)) $rel)
        MkDirSafe (Split-Path -Parent $dest)
        Copy-Item -LiteralPath $_.FullName -Destination $dest -Force
      }
  }
}

# Collect from roots
foreach ($r in $roots) {
  Write-Verbose "Collecting sources/logs from $r"
  Copy-Matches $r $IncludePatterns
  Copy-Matches $r $UserLogPicks
}

# Config from local build
Copy-Matches "." $ConfigPicks

# Optional binaries
if ($IncludeBuild) {
  Write-Verbose "Including build artifacts"
  Copy-Matches "." $BinaryPicks
}

# Environment snapshot
$envTxt = @()
$envTxt += "Timestamp: $(Get-Date -Format o)"
$envTxt += ""
$envTxt += "OS:"
$envTxt += (Get-CimInstance Win32_OperatingSystem | Select-Object Caption, Version, BuildNumber | Format-List | Out-String)
$envTxt += ""
$envTxt += "Git:"
try { $envTxt += (& git --version) } catch { $envTxt += "git not found" }
$envTxt += ""
$envTxt += "Repo status:"
try { $envTxt += (& git status --porcelain=v1 -b) } catch { $envTxt += "n/a" }
$envTxt += ""
$envTxt += "CMake:"
try { $envTxt += (& cmake --version) } catch { $envTxt += "cmake not found" }
$envTxt += ""
$envTxt += "MSBuild:"
try { $envTxt += (& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" -version) } catch { $envTxt += "msbuild not found" }

$envPath = Join-Path $stage "ENVIRONMENT.txt"
$envTxt | Set-Content -Encoding UTF8 -LiteralPath $envPath

if ($NoZip) {
  Write-Host "Staged bundle ready (no ZIP):" -ForegroundColor Green
  Write-Host "  $stage"
  exit 0
}

# Zip it
if (Test-Path -LiteralPath $zipPath) { Remove-Item -LiteralPath $zipPath -Force }
Write-Verbose "Compressing to $zipPath"
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zipPath -Force

Write-Host ""; Write-Host "Parser debug ZIP created:" -ForegroundColor Green
Write-Host "  $zipPath"
# Optional: clean staging
Remove-Item -LiteralPath $stage -Recurse -Force
```

---

## Make-ParserDebugZip-Fast.ps1

```powershell
<#
  Make-ParserDebugZip-Fast.ps1
  Ultra-fast: bundle ONLY git-tracked files using `git archive`.
  Creates releases\parser-debug\parser-debug-git-<rev>-<timestamp>.zip
#>

[CmdletBinding()]
param(
  [string]$OutDir = ".\releases\parser-debug"
)

$ErrorActionPreference = "Stop"

# Ensure we are in a git repo
try { $null = git rev-parse --is-inside-work-tree 2>$null } catch {
  throw "Not a git repository. Run from your repo root."
}

$rev = (git rev-parse --short HEAD).Trim()
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$newDir = Join-Path $OutDir .
New-Item -ItemType Directory -Path $newDir -Force | Out-Null
$zip = Join-Path $newDir ("parser-debug-git-$rev-$stamp.zip")

# Create the archive
& git archive -o $zip --format=zip HEAD

Write-Host "Git archive created:" -ForegroundColor Green
Write-Host "  $zip"
```

---

### Notes

* Use `-Verbose` to see progress (counts and steps) in the full script.
* Use `-NoZip` if compression or AV is slowing you down; you can zip the staging folder manually later.
* If your repos are in custom paths, run with full paths or `Set-Location` first.
