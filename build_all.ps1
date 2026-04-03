param(
    [string]$RepoRoot = "D:\code\ccode",
    [string]$BuildDir = "D:\code\ccode\build",
    [string]$Config = "Release",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

function Write-Step {
    param([string]$Text)
    Write-Host ""
    Write-Host "============================================================"
    Write-Host $Text
    Write-Host "============================================================"
}

function Require-Path {
    param(
        [string]$Path,
        [string]$Label
    )
    if (-not (Test-Path $Path)) {
        throw "Missing $Label: $Path"
    }
}

function Run-Checked {
    param(
        [string]$FilePath,
        [string[]]$Arguments,
        [string]$Label,
        [string]$WorkingDirectory = $PWD.Path
    )

    Write-Host ""
    Write-Host ">>> $Label"
    Write-Host "$FilePath $($Arguments -join ' ')"

    Push-Location $WorkingDirectory
    try {
        & $FilePath @Arguments
        if ($LASTEXITCODE -ne 0) {
            throw "$Label failed with exit code $LASTEXITCODE"
        }
    }
    finally {
        Pop-Location
    }
}

$RepoRoot = (Resolve-Path $RepoRoot).Path
Require-Path -Path $RepoRoot -Label "repo root"

$WinCMakeLists = Join-Path $RepoRoot "CMakeLists.txt"
Require-Path -Path $WinCMakeLists -Label "top-level CMakeLists.txt"

$WslBuildScriptWin = Join-Path $RepoRoot "wsl_build_dottalkpp.sh"
Require-Path -Path $WslBuildScriptWin -Label "WSL build script"

$DistroRepoRoot = "/mnt/d/code/ccode"
$WslBuildScriptLinux = "/mnt/d/code/ccode/wsl_build_dottalkpp.sh"

Write-Step "DotTalk++ dual build starting"

Write-Host "RepoRoot : $RepoRoot"
Write-Host "BuildDir : $BuildDir"
Write-Host "Config   : $Config"
Write-Host "Clean    : $Clean"

if ($Clean) {
    Write-Step "Cleaning Windows build directory"
    if (Test-Path $BuildDir) {
        Remove-Item -Recurse -Force $BuildDir
    }
}

Write-Step "Windows configure"
Run-Checked -FilePath "cmake" -Arguments @(
    "-S", $RepoRoot,
    "-B", $BuildDir
) -Label "Windows CMake configure"

Write-Step "Windows build"
Run-Checked -FilePath "cmake" -Arguments @(
    "--build", $BuildDir,
    "--config", $Config
) -Label "Windows build"

Write-Step "WSL build script prep"
Run-Checked -FilePath "wsl" -Arguments @(
    "bash", "-lc", "sed -i 's/\r$//' '$WslBuildScriptLinux' && chmod +x '$WslBuildScriptLinux'"
) -Label "Normalize and chmod WSL build script"

Write-Step "WSL build"
Run-Checked -FilePath "wsl" -Arguments @(
    "bash", "-lc", "'$WslBuildScriptLinux'"
) -Label "WSL build"

Write-Step "Dual build complete"
Write-Host "Windows build: OK"
Write-Host "WSL build    : OK"