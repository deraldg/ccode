[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Config = 'Release',
    [string[]]$Targets = @('dottalkpp', 'pydottalk'),
    [ValidateSet('Win32', 'x64', 'ARM64')]
    [string]$Arch = 'x64',
    [switch]$Clean,
    [switch]$SkipWsl
)

$ErrorActionPreference = 'Stop'
$RepoRoot = (Resolve-Path $PSScriptRoot).Path
$VcpkgRoot = "C:\Users\deral\vcpkg"
$VcpkgToolchain = "$VcpkgRoot\scripts\buildsystems\vcpkg.cmake"
$VsDevCmd = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"

function Stop-IntelliSenseLocks {
    Get-Process vctip -ErrorAction SilentlyContinue |
        Stop-Process -Force -ErrorAction SilentlyContinue | Out-Null
}

function Ensure-Dir($path) {
    if (-not (Test-Path $path)) {
        New-Item -ItemType Directory -Path $path | Out-Null
    }
}

function Run-CMake($S, $B, $Args) {
    Write-Host ">>> Configure: $B" -ForegroundColor Yellow
    Ensure-Dir $B
    $null = & cmake -S $S -B $B @Args | Out-Host
    if ($LASTEXITCODE -ne 0) {
        Write-Error "CMake configuration failed for $B"
        exit 1
    }
}

function Build-Targets($B, $Cfg, $Tgts) {
    foreach ($t in $Tgts) {
        Write-Host ">>> Build: $t ($Cfg) in $B" -ForegroundColor Cyan
        $null = & cmake --build $B --config $Cfg --target $t | Out-Host
        if ($LASTEXITCODE -ne 0) {
            Write-Error "Build failed for target $t in $B"
            exit 1
        }
    }
}

function Clean-Dir($B) {
    if (Test-Path $B) {
        Write-Host ">>> Cleaning $B" -ForegroundColor DarkGray
        Remove-Item -Recurse -Force $B
    }
}

function Win-Ninja-Build {
    param([string]$Config, [string[]]$Targets, [switch]$Clean)
    $B = Join-Path $RepoRoot 'build'
    if ($Clean) { Clean-Dir $B }
    Run-CMake $RepoRoot $B @('-G', 'Ninja Multi-Config', '-DCMAKE_TOOLCHAIN_FILE:PATH=' + $VcpkgToolchain)
    Build-Targets $B $Config $Targets
    return $B
}

function Win-Msvc-Build {
    param([string]$Config, [string[]]$Targets, [string]$Arch, [switch]$Clean)
    $B = Join-Path $RepoRoot 'build-msvc'
    if ($Clean) { Clean-Dir $B }
    # Set up Visual Studio environment
    if (Test-Path $VsDevCmd) {
        Write-Host "Setting up Visual Studio environment..." -ForegroundColor Yellow
        cmd /c "$VsDevCmd -arch=$Arch & set" | ForEach-Object {
            if ($_ -match "^(.*?)=(.*)$") {
                Set-Item -Path "Env:$($matches[1])" -Value $matches[2]
            }
        }
    } else {
        Write-Error "VsDevCmd.bat not found at $VsDevCmd"
        exit 1
    }
    Run-CMake $RepoRoot $B @('-G', 'Visual Studio 17 2022', '-A', $Arch, '-DCMAKE_TOOLCHAIN_FILE:PATH=' + $VcpkgToolchain)
    Build-Targets $B $Config $Targets
    return $B
}

function Convert-ToWslPath([string]$winPath) {
    $p = & wsl.exe sh -lc "wslpath -a -u '$(($winPath -replace "'","'\\''"))'"
    return $p.Trim()
}

function Wsl-Build {
    param([string]$Config, [string[]]$Targets, [switch]$Clean)
    if ($SkipWsl) { return $null }
    $WslRepo = Convert-ToWslPath $RepoRoot
    $WslVcpkg = Convert-ToWslPath $VcpkgRoot
    $WslBuild = "$WslRepo/build-wsl"
    if ($Clean) { & wsl.exe sh -lc "rm -rf '$WslBuild'" | Out-Host }
    # Check for dependencies in WSL
    & wsl.exe sh -lc "command -v cmake >/dev/null || echo '!! cmake not found in WSL (apt install cmake)'" | Out-Host
    & wsl.exe sh -lc "command -v ninja >/dev/null || echo '!! ninja not found in WSL (apt install ninja-build)'" | Out-Host
    Write-Host ">>> Configure (WSL, Ninja)" -ForegroundColor Yellow
    $cfgCmd = @"
set -e
mkdir -p '$WslBuild'
cmake -S '$WslRepo' -B '$WslBuild' -G Ninja -DCMAKE_BUILD_TYPE=$Config -DCMAKE_TOOLCHAIN_FILE='$WslVcpkg/scripts/buildsystems/vcpkg.cmake'
"@
    $null = & wsl.exe sh -lc "$cfgCmd" | Out-Host
    if ($LASTEXITCODE -ne 0) {
        Write-Error "CMake configuration failed for WSL"
        exit 1
    }
    foreach ($t in $Targets) {
        Write-Host ">>> Build (WSL): $t ($Config)" -ForegroundColor Cyan
        $null = & wsl.exe sh -lc "cmake --build '$WslBuild' --target $t" | Out-Host
        if ($LASTEXITCODE -ne 0) {
            Write-Error "Build failed for target $t in WSL"
            exit 1
        }
    }
    return $WslBuild
}

Write-Host "=============== Rebuild Matrix ==============" -ForegroundColor Green
Write-Host "Repo: $RepoRoot"
Write-Host "Config: $Config"
Write-Host "Arch: $Arch (MSVC only)"
Write-Host "Targets: $($Targets -join ', ')"
Write-Host "Clean: $Clean"
Write-Host "WSL: $([bool](-not $SkipWsl))"
Write-Host "============================================="

Stop-IntelliSenseLocks

# Ensure vcpkg is bootstrapped
if (-not (Test-Path "$VcpkgRoot\vcpkg.exe")) {
    Write-Host "Bootstrapping vcpkg..." -ForegroundColor Yellow
    & "$VcpkgRoot\bootstrap-vcpkg.bat" | Out-Host
}
Write-Host "Installing tvision via vcpkg..." -ForegroundColor Yellow
& "$VcpkgRoot\vcpkg" install tvision:x64-windows | Out-Host

$built = @()
$built += Win-Ninja-Build -Config $Config -Targets $Targets -Clean:$Clean
$built += Win-Msvc-Build -Config $Config -Targets $Targets -Arch $Arch -Clean:$Clean
$wslB = Wsl-Build -Config $Config -Targets $Targets -Clean:$Clean
if ($wslB) { $built += $wslB }

Write-Host "`nBuild roots:" -ForegroundColor Green
$built | Where-Object { $_ } | ForEach-Object { " - $_" } | Out-Host

# Show Windows exes
$winExes = @()
$path1 = Join-Path $RepoRoot "build\$Config\dottalkpp.exe"
$path2 = Join-Path $RepoRoot "build-msvc\$Config\dottalkpp.exe"
if (Test-Path $path1) { $winExes += $path1 }
if (Test-Path $path2) { $winExes += $path2 }
if ($winExes.Count) {
    Write-Host "`nWindows exes:" -ForegroundColor Green
    $winExes | ForEach-Object { " - $_" } | Out-Host
}

# Show Windows pyds
$winPyds = @()
$dir1 = Join-Path $RepoRoot "build\$Config"
$dir2 = Join-Path $RepoRoot "build-msvc\$Config"
if (Test-Path $dir1) { $winPyds += Get-ChildItem -Path $dir1 -Filter "pydottalk.*.pyd" -ErrorAction SilentlyContinue }
if (Test-Path $dir2) { $winPyds += Get-ChildItem -Path $dir2 -Filter "pydottalk.*.pyd" -ErrorAction SilentlyContinue }
if ($winPyds.Count) {
    Write-Host "`nWindows pyds:" -ForegroundColor Green
    $winPyds | ForEach-Object { " - " + $_.FullName } | Out-Host
}

# Show WSL exes
$wslExes = @()
if (-not $SkipWsl) {
    $wslExe = & wsl.exe sh -lc "find '$((Convert-ToWslPath $RepoRoot)/build-wsl)' -type f -name 'dottalkpp'"
    if ($wslExe) { $wslExes += $wslExe }
    if ($wslExes.Count) {
        Write-Host "`nWSL exes:" -ForegroundColor Green
        $wslExes | ForEach-Object { " - $_" } | Out-Host
    }
}

Write-Host "`nDone." -ForegroundColor Green