# ================================
#  DotTalk++ Dev Environment Setup
#  Surface 64GB + SD Card Strategy
# ================================

# --- CONFIG ---
$SD = "D:"   # Change if your SD card uses a different letter
$User = $env:USERNAME

Write-Host "Using SD card at $SD" -ForegroundColor Cyan

# --- 1. CREATE DIRECTORY STRUCTURE ---
$paths = @(
    "$SD\dev",
    "$SD\dev\python_envs",
    "$SD\dev\tools",
    "$SD\dev\scratch",
    "$SD\dev\dottalkpp",     # You will manually copy into here
    "$SD\data",
    "$SD\data\db",
    "$SD\data\exports",
    "$SD\data\pipcache",
    "$SD\docs",
    "$SD\docs\notes",
    "$SD\docs\specs",
    "$SD\docs\drafts",
    "$SD\media",
    "$SD\media\images",
    "$SD\media\reference",
    "$SD\downloads"
)

foreach ($p in $paths) {
    if (-not (Test-Path $p)) {
        New-Item -ItemType Directory -Path $p | Out-Null
        Write-Host "Created: $p"
    } else {
        Write-Host "Exists:  $p"
    }
}

# --- 2. REDIRECT USER FOLDERS ---
function Redirect-Folder($folderName, $newPath) {
    $shell = New-Object -ComObject Shell.Application
    $folder = $shell.Namespace($folderName)
    if ($folder) {
        Write-Host "Redirecting $folderName to $newPath"
        (New-Object -ComObject WScript.Shell).RegWrite(
            "HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\User Shell Folders\$folderName",
            $newPath,
            "REG_EXPAND_SZ"
        )
    }
}

Redirect-Folder "Personal"   "$SD\docs"        # Documents
Redirect-Folder "My Pictures" "$SD\media\images"
Redirect-Folder "{374DE290-123F-4565-9164-39C4925E467B}" "$SD\downloads"  # Downloads

Write-Host "User folders redirected." -ForegroundColor Green

# --- 3. CREATE PYTHON 3.12 VENV ON SD ---
$venvPath = "$SD\dev\python_envs\dottalkpp312"

if (-not (Test-Path $venvPath)) {
    Write-Host "Creating Python 3.12 venv at $venvPath" -ForegroundColor Cyan
    python -m venv $venvPath
} else {
    Write-Host "Python venv already exists at $venvPath"
}

# --- 4. SET PIP CACHE TO SD ---
$profileSnippet = @"
# --- DotTalk++ Dev Environment Settings ---
# Redirect pip cache to SD card
\$env:PIP_CACHE_DIR = '$SD\data\pipcache'

# Activate default DotTalk++ venv
function work-dottalkpp {
    & '$venvPath\Scripts\activate'
    Set-Location '$SD\dev\dottalkpp'
}
"@

$profilePath = "$HOME\Documents\PowerShell\Microsoft.PowerShell_profile.ps1"

if (-not (Test-Path $profilePath)) {
    New-Item -ItemType File -Path $profilePath -Force | Out-Null
}

if (-not (Get-Content $profilePath | Select-String "DotTalk\+\+ Dev Environment Settings")) {
    Add-Content -Path $profilePath -Value $profileSnippet
    Write-Host "PowerShell profile updated." -ForegroundColor Green
} else {
    Write-Host "PowerShell profile already contains environment settings."
}

# --- 5. CREATE VS CODE WORKSPACE FOLDER ---
$workspaceFolder = "$SD\dev\dottalkpp\.vscode"
if (-not (Test-Path $workspaceFolder)) {
    New-Item -ItemType Directory -Path $workspaceFolder | Out-Null
}

$settingsJson = @"
{
    "python.defaultInterpreterPath": "$venvPath\\Scripts\\python.exe",
    "terminal.integrated.defaultProfile.windows": "PowerShell",
    "terminal.integrated.cwd": "$SD\\dev\\dottalkpp"
}
"@

$settingsPath = "$workspaceFolder\settings.json"
$settingsJson | Out-File -FilePath $settingsPath -Encoding UTF8

Write-Host "VS Code workspace configured." -ForegroundColor Green

# --- DONE ---
Write-Host "`nEnvironment setup complete." -ForegroundColor Cyan
Write-Host "Manually copy your DotTalk++ repo into: $SD\dev\dottalkpp"