
[CmdletBinding()]
param(
  [string]$RepoRoot = (Get-Location).Path,
  [string]$OutDir = "$PWD\_debug",
  [string]$ExePath,
  [string[]]$IncludeGlobs = @()
)

$ErrorActionPreference = "Stop"
$ts = Get-Date -Format "yyyyMMdd-HHmmss"
$RepoRoot = (Resolve-Path -LiteralPath $RepoRoot).Path
if (-not (Test-Path -LiteralPath $RepoRoot)) { throw "RepoRoot not found: $RepoRoot" }

# Prepare output folders
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$stage = Join-Path $OutDir "parser-debug-$ts"
New-Item -ItemType Directory -Force -Path $stage | Out-Null

Write-Verbose "RepoRoot = $RepoRoot"
Write-Verbose "OutDir   = $OutDir"
Write-Verbose "Stage    = $stage"

function Add-Text {
  param([string]$RelPath, [string]$Content)
  $dest = Join-Path $stage $RelPath
  New-Item -ItemType Directory -Force -Path (Split-Path $dest) | Out-Null
  $Content | Out-File -FilePath $dest -Encoding UTF8
}

function Copy-Rel {
  param([string]$RelSource, [string]$RelDest)
  $src = Join-Path $RepoRoot $RelSource
  $dst = Join-Path $stage $RelDest
  if (Test-Path -LiteralPath $src) {
    New-Item -ItemType Directory -Force -Path (Split-Path $dst) | Out-Null
    Copy-Item -LiteralPath $src -Destination $dst -Recurse -Force -ErrorAction SilentlyContinue
    Write-Verbose "Copied $RelSource -> $RelDest"
  } else {
    Write-Verbose "Skip (not found): $RelSource"
  }
}

# 1) Basic repo summary
Add-Text "SUMMARY.txt" (@"
Parser Debug Collection
=======================
Timestamp : $ts
RepoRoot  : $RepoRoot

Host      : $env:COMPUTERNAME
User      : $env:USERNAME
PSVersion : $($PSVersionTable.PSVersion)

"@)

# 2) Collect CMake files and build logs
Copy-Rel "CMakeLists.txt" "cmake\root\CMakeLists.txt"
# Recursively collect all CMakeLists.txt
Get-ChildItem -Path $RepoRoot -Recurse -Filter "CMakeLists.txt" -ErrorAction SilentlyContinue |
  ForEach-Object {
    $rel = $_.FullName.Substring($RepoRoot.Length).TrimStart('\','/')
    $dest = Join-Path $stage ("cmake\all\" + $rel)
    New-Item -ItemType Directory -Force -Path (Split-Path $dest) | Out-Null
    Copy-Item -LiteralPath $_.FullName -Destination $dest -Force
  }

Copy-Rel "build\CMakeCache.txt" "build\CMakeCache.txt"
Copy-Rel "build\CMakeFiles\CMakeOutput.log" "build\CMakeFiles\CMakeOutput.log"
Copy-Rel "build\CMakeFiles\CMakeError.log"  "build\CMakeFiles\CMakeError.log"

# 3) Parser/CLI & headers (common locations)
Copy-Rel "src\cli" "src\cli"
Copy-Rel "include" "include"

# 4) Build artifacts of interest
Copy-Rel "compile_commands.json" "build\compile_commands.json"
Get-ChildItem -Path $RepoRoot -Filter "*.sln","*.vcxproj" -File -ErrorAction SilentlyContinue |
  ForEach-Object {
    $rel = $_.FullName.Substring($RepoRoot.Length).TrimStart('\','/')
    $dest = Join-Path $stage ("build\" + $rel)
    New-Item -ItemType Directory -Force -Path (Split-Path $dest) | Out-Null
    Copy-Item -LiteralPath $_.FullName -Destination $dest -Force
  }

# 5) Git + toolchain versions
$gitInfo = New-Object System.Text.StringBuilder
try {
  $null = git -C $RepoRoot rev-parse --is-inside-work-tree 2>$null
  if ($LASTEXITCODE -eq 0) {
    $gitInfo.AppendLine((git -C $RepoRoot rev-parse HEAD)) | Out-Null
    $gitInfo.AppendLine("") | Out-Null
    $gitInfo.AppendLine((git -C $RepoRoot status --porcelain=v1 -b)) | Out-Null
    $gitInfo.AppendLine("") | Out-Null
    $gitInfo.AppendLine((git -C $RepoRoot log -n 10 --oneline)) | Out-Null
  } else {
    $gitInfo.AppendLine("Not a git repo or git unavailable.") | Out-Null
  }
} catch {
  $gitInfo.AppendLine("git not found.") | Out-Null
}
Add-Text "env\git-info.txt" $gitInfo.ToString()

function Tool-Version($cmd, $args) {
  try {
    $p = Start-Process -FilePath $cmd -ArgumentList $args -NoNewWindow -RedirectStandardOutput -PassThru -WorkingDirectory $RepoRoot
    $p.WaitForExit()
    return [System.IO.File]::ReadAllText($p.StandardOutput.BaseStream.Name)
  } catch {
    return "$cmd not found."
  }
}

Add-Text "env\cmake-version.txt" (Tool-Version "cmake" "--version")
# cl.exe prints version to stderr; invoke via cmd to capture
try {
  $clVer = cmd /c "cl" 2>&1
} catch { $clVer = "cl (MSVC) not found." }
Add-Text "env\msvc-cl-version.txt" $clVer

Add-Text "env\ninja-version.txt" (Tool-Version "ninja" "--version")

# PATH snapshot (trimmed)
$shortPath = ($env:PATH -split ';' | Select-Object -Unique) -join [Environment]::NewLine
Add-Text "env\PATH.txt" $shortPath

# 6) Optional tiny runtime trace (HELP only) if ExePath provided
if ($ExePath) {
  $exe = Resolve-Path -LiteralPath $ExePath
  $cmds = @("HELP","QUIT")
  $cmdFile = Join-Path $stage "runtime\commands.txt"
  New-Item -ItemType Directory -Force -Path (Split-Path $cmdFile) | Out-Null
  $cmds -join [Environment]::NewLine | Out-File -FilePath $cmdFile -Encoding ASCII

  $outFile = Join-Path $stage "runtime\output.txt"
  Write-Verbose "Running runtime trace via $exe"
  # Pipe commands into the CLI; capture both streams
  Get-Content $cmdFile | & $exe 2>&1 | Tee-Object -FilePath $outFile | Out-Null
}

# 7) Include any extra globs the caller passed
foreach ($g in $IncludeGlobs) {
  Write-Verbose "Including extra glob: $g"
  Get-ChildItem -Path (Join-Path $RepoRoot $g) -Recurse -File -ErrorAction SilentlyContinue |
    ForEach-Object {
      $rel = $_.FullName.Substring($RepoRoot.Length).TrimStart('\','/')
      $dest = Join-Path $stage ("extra\" + $rel)
      New-Item -ItemType Directory -Force -Path (Split-Path $dest) | Out-Null
      Copy-Item -LiteralPath $_.FullName -Destination $dest -Force
    }
}

# 8) Create the final ZIP
$zipPath = Join-Path $OutDir "parser-debug-$ts.zip"
if (Test-Path -LiteralPath $zipPath) { Remove-Item -LiteralPath $zipPath -Force }
Write-Verbose "Zipping: $zipPath"
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zipPath -CompressionLevel Optimal

Write-Host "`nCreated: $zipPath"
