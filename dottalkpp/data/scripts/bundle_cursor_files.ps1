param(
    [string]$RepoRoot = "D:\code\ccode",
    [string]$OutName = ""
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $RepoRoot)) {
    throw "Repo root not found: $RepoRoot"
}

$files = @(
    "src\xbase\cursor_hook.hpp",
    "src\xbase\cursor_hook.cpp",
    "src\cli\cursor_status.hpp",
    "src\cli\cursor_status.cpp",
    "src\xbase\xbase.hpp",
    "src\xbase\df_file.cpp",
    "src\cli\cmd_init.cpp",
    "src\xbase\CMakeLists.txt",
    "src\CMakeLists.txt",
    "CMakeLists.txt",
    "include\xbase\cursor_hook.hpp",
    "include\xbase\cursor_status.hpp",
    "include\xbase.hpp",
    "include\cli\cmd_init.hpp"
)

$found = New-Object System.Collections.Generic.List[string]
$missing = New-Object System.Collections.Generic.List[string]

foreach ($rel in $files) {
    $full = Join-Path $RepoRoot $rel
    if (Test-Path $full) {
        $found.Add($rel)
    } else {
        $missing.Add($rel)
    }
}

if ($found.Count -eq 0) {
    throw "None of the requested files were found under $RepoRoot"
}

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
if ([string]::IsNullOrWhiteSpace($OutName)) {
    $zipPath = Join-Path $RepoRoot "cursor_bundle_$stamp.zip"
} else {
    if ($OutName.ToLower().EndsWith(".zip")) {
        $zipPath = Join-Path $RepoRoot $OutName
    } else {
        $zipPath = Join-Path $RepoRoot ($OutName + ".zip")
    }
}

$tempRoot = Join-Path $env:TEMP ("cursor_bundle_" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $tempRoot | Out-Null

try {
    foreach ($rel in $found) {
        $src = Join-Path $RepoRoot $rel
        $dst = Join-Path $tempRoot $rel
        $dstDir = Split-Path $dst -Parent
        New-Item -ItemType Directory -Force -Path $dstDir | Out-Null
        Copy-Item -LiteralPath $src -Destination $dst
    }

    if (Test-Path $zipPath) {
        Remove-Item -LiteralPath $zipPath -Force
    }

    Compress-Archive -Path (Join-Path $tempRoot "*") -DestinationPath $zipPath -Force

    Write-Host ""
    Write-Host "ZIP created:" -ForegroundColor Green
    Write-Host "  $zipPath"
    Write-Host ""
    Write-Host "Included files ($($found.Count)):" -ForegroundColor Cyan
    $found | ForEach-Object { Write-Host "  $_" }

    Write-Host ""
    if ($missing.Count -gt 0) {
        Write-Host "Missing files ($($missing.Count)):" -ForegroundColor Yellow
        $missing | ForEach-Object { Write-Host "  $_" }
    } else {
        Write-Host "No requested files were missing." -ForegroundColor Green
    }
}
finally {
    if (Test-Path $tempRoot) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force
    }
}