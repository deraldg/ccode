# run-pydottalk-smoke.ps1
# Launches pydottalk_smoke.py with correct Python 3.12 and paths

# ────────────────────────────────────────────────
#          Customize these paths if needed
# ────────────────────────────────────────────────

$REPO_ROOT          = "D:\code\ccode"                                # ← your repo root
$PYTHON_VCPKG       = "$REPO_ROOT\build\vcpkg_installed\x64-windows\tools\python3\python.exe"
$PYDOTTALK_BUILD_DIR = "$REPO_ROOT\build\bindings\pydottalk\Release"   # where pydottalk.cp312-win_amd64.pyd lives
$SMOKE_SCRIPT       = "$REPO_ROOT\bindings\pydottalk_smoke.py"         # or move it wherever you want

# Optional: override DBF folder via env var (used by smoke test)
# $env:DOTTALK_DBF_DIR = "$REPO_ROOT\dottalkpp\data\dbf"

# ────────────────────────────────────────────────
#               Basic validation
# ────────────────────────────────────────────────

if (-not (Test-Path $PYTHON_VCPKG)) {
    Write-Host "Python 3.12 not found at:`n  $PYTHON_VCPKG" -ForegroundColor Red
    Write-Host "Check vcpkg installation path or edit `$PYTHON_VCPKG` in this script." -ForegroundColor Red
    exit 1
}

if (-not (Test-Path $PYDOTTALK_BUILD_DIR)) {
    Write-Host "pydottalk build folder not found:`n  $PYDOTTALK_BUILD_DIR" -ForegroundColor Yellow
    Write-Host "Make sure you built the Release configuration." -ForegroundColor Yellow
}

if (-not (Test-Path $SMOKE_SCRIPT)) {
    Write-Host "Smoke test script not found:`n  $SMOKE_SCRIPT" -ForegroundColor Red
    exit 1
}

# ────────────────────────────────────────────────
#                 Set up environment
# ────────────────────────────────────────────────

$oldPythonPath = $env:PYTHONPATH

# Prepend build dir so Python finds the fresh .pyd first
$env:PYTHONPATH = "$PYDOTTALK_BUILD_DIR" + $(if ($env:PYTHONPATH) {";$env:PYTHONPATH"} else {""})

Write-Host "Using Python:       " -NoNewline -ForegroundColor Cyan
& $PYTHON_VCPKG --version | Write-Host -ForegroundColor White

Write-Host "PYTHONPATH:" -ForegroundColor Cyan
Write-Host "  $env:PYTHONPATH" -ForegroundColor DarkGray

Write-Host "`nLaunching smoke test ..." -ForegroundColor Green
Write-Host "─" * 60 -ForegroundColor DarkGray

# Run the smoke test
& $PYTHON_VCPKG $SMOKE_SCRIPT

# ────────────────────────────────────────────────
#                    Cleanup
# ────────────────────────────────────────────────

$env:PYTHONPATH = $oldPythonPath

Write-Host "─" * 60 -ForegroundColor DarkGray
Write-Host "Done." -ForegroundColor Green