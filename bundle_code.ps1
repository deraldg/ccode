# bundle_code.ps1
# Make a versioned ZIP with manifest and optional split parts (ASCII-safe)

[CmdletBinding()]
param(
  [string]$ProjectPath = ".",
  [string]$Version = "alpha.3",
  [string]$OutDir = "dist",
  [int]$SplitMB = 0
)

function Write-Info($msg){ Write-Host "[*] $msg" -ForegroundColor Cyan }
function Write-OK($msg)  { Write-Host "[OK] $msg" -ForegroundColor Green }
function Write-Warn($msg){ Write-Host "[!] $msg" -ForegroundColor Yellow }

$ExcludeNames = @(
  ".git",".hg",".svn","venv",".venv","env",".env","node_modules",
  "__pycache__", ".mypy_cache",".pytest_cache",".ruff_cache",
  ".idea",".vscode",".DS_Store","Thumbs.db","dist"
)
$ExcludePatterns = @("*.pyc","*.pyo","*.log","*.tmp","*.bak","*.swp","*.orig")

# Resolve paths
$ProjectPath = (Resolve-Path $ProjectPath).Path
$projName = Split-Path $ProjectPath -Leaf
$projSlug = ($projName -replace '[^\w\.-]','_')
$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$bundleBase = "$projSlug-$Version-$timestamp"
$artifactDir = Join-Path $ProjectPath $OutDir
$zipPath = Join-Path $artifactDir "$bundleBase.zip"
$manifestPath = Join-Path $artifactDir "$bundleBase.manifest.txt"

function Test-ExcludePath {
  param([string]$FullPath)
  $segments = ($FullPath -replace '\\','/').Split('/')
  foreach ($seg in $segments) { if ($ExcludeNames -contains $seg) { return $true } }
  foreach ($pat in $ExcludePatterns) {
    if (Test-Path -LiteralPath $FullPath -PathType Leaf) {
      if ([IO.Path]::GetFileName($FullPath) -like $pat) { return $true }
    }
  }
  return $false
}

New-Item -ItemType Directory -Force -Path $artifactDir | Out-Null

Write-Info "Project: $projName"
Write-Info "Version: $Version"
Write-Info "Output : $artifactDir"

Push-Location $ProjectPath
try {
  Write-Info "Scanning files…"
  $allFiles = Get-ChildItem -Recurse -File
  $keep = foreach ($f in $allFiles) { if (-not (Test-ExcludePath -FullPath $f.FullName)) { $f } }
  if (-not $keep) { throw "No files to bundle. Check exclusions or path." }

  $rel = $keep | ForEach-Object { $_.FullName.Substring($PWD.Path.Length + 1) }

  if (Test-Path $zipPath) { Remove-Item $zipPath -Force }
  Write-Info "Creating zip: $zipPath"
  Compress-Archive -Path $rel -DestinationPath $zipPath -CompressionLevel Optimal
  Write-OK "Zip created"

  # Compute sizes separately to avoid inline parse issues
  $totalBytes = ($keep | Measure-Object Length -Sum).Sum
  $totalMB = [math]::Round($totalBytes / 1MB, 2)
  $zipMB = [math]::Round((Get-Item -LiteralPath $zipPath).Length / 1MB, 2)
  $fileCount = $keep.Count
  $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $zipPath).Hash

  $manifest = @()
  $manifest += "Project : $projName"
  $manifest += "Version : $Version"
  $manifest += "Created : $(Get-Date -Format 'u')"
  $manifest += "Bundle  : $(Split-Path -Leaf $zipPath)"
  $manifest += "Files   : $fileCount"
  $manifest += "Size    : $totalMB MB (unzipped); $zipMB MB (zip)"
  $manifest += "SHA256  : $hash"
  $manifest += ""
  $manifest += "---- INCLUDED FILES (relative) ----"
  $manifest += ($rel | Sort-Object)

  $manifest -join "`r`n" | Out-File -FilePath $manifestPath -Encoding UTF8 -Force
  Write-OK "Manifest written: $(Split-Path -Leaf $manifestPath)"

  if ($SplitMB -gt 0) {
    $partDir = Join-Path $artifactDir "$bundleBase.parts"
    New-Item -ItemType Directory -Force -Path $partDir | Out-Null
    Write-Info "Splitting zip into $SplitMB MB parts…"

    $bytesPerPart = $SplitMB * 1MB
    $in = [IO.File]::OpenRead($zipPath)
    try {
      $buffer = New-Object byte[] 1048576  # 1 MB
      $idx = 1
      $bytesWrittenThisPart = 0
      $out = $null
      $currentPath = $null
      function New-PartStream([int]$i, [string]$zipPath, [string]$partDir) {
        $name = (Split-Path -Leaf $zipPath)
        $p = Join-Path $partDir ("{0}.part{1:D3}" -f $name, $i)
        return ,([IO.File]::Open($p,[IO.FileMode]::Create,[IO.FileAccess]::Write,[IO.FileShare]::None), $p)
      }
      $tmp = New-PartStream $idx $zipPath $partDir
      $out = $tmp[0]; $currentPath = $tmp[1]
      try {
        while (($read = $in.Read($buffer, 0, $buffer.Length)) -gt 0) {
          if ($bytesWrittenThisPart + $read -gt $bytesPerPart) {
            $remain = $bytesPerPart - $bytesWrittenThisPart
            if ($remain -gt 0) { $out.Write($buffer, 0, $remain) }
            $out.Dispose()
            $parts += $currentPath
            $idx++
            $tmp = New-PartStream $idx $zipPath $partDir
            $out = $tmp[0]; $currentPath = $tmp[1]
            $out.Write($buffer, $remain, $read - $remain)
            $bytesWrittenThisPart = $read - $remain
          } else {
            $out.Write($buffer, 0, $read)
            $bytesWrittenThisPart += $read
          }
        }
      } finally {
        if ($out) { $out.Dispose(); $parts += $currentPath }
      }
    } finally {
      $in.Dispose()
    }

@"
Parts created from: $(Split-Path -Leaf $zipPath)
To reassemble on Windows PowerShell:

  Get-ChildItem -Path "$partDir" -Filter "*.part*" | Sort-Object Name |
    ForEach-Object { Get-Content -Path $_.FullName -Encoding Byte -ReadCount 0 } |
    Set-Content -Path "$($zipPath)" -Encoding Byte

Or with cmd:
  copy /b "$(Split-Path -Leaf $zipPath).part001"+"$(Split-Path -Leaf $zipPath).part002"+... "$($bundleBase)-reassembled.zip"
"@ | Out-File -FilePath (Join-Path $partDir "READ_ME.txt") -Encoding UTF8 -Force

    Write-OK "Created split parts in: $partDir"
  }

  Write-OK "Done."
  Write-Host ""
  Write-Host "Artifacts:" -ForegroundColor Magenta
  Write-Host "  $zipPath"
  Write-Host "  $manifestPath"
  if (Test-Path (Join-Path $artifactDir "$bundleBase.parts")) {
    Write-Host "  $(Join-Path $artifactDir "$bundleBase.parts") (split parts + READ_ME.txt)"
  }

} finally {
  Pop-Location
}
