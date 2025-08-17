<# 
.SYNOPSIS
  Initialize/repair GitHub remote and push current repo.

.PARAMETER Owner
  Your GitHub username (e.g., deraldg)

.PARAMETER RepoName
  Repository name on GitHub (e.g., ccode)

.PARAMETER Private
  Create the repo as private (default: public)

.PARAMETER PushTags
  Also push tags (pushes 'alpha-v3' if present, and can push all tags if -AllTags)

.PARAMETER AllTags
  Push all tags (implies -PushTags)

.PARAMETER NoCreate
  Do NOT attempt to create the repo on GitHub; only set remote and push.

.EXAMPLE
  ./githelpme.ps1 -Owner deraldg -RepoName ccode -Private -PushTags
#>

param(
  [Parameter(Mandatory=$true)][string]$Owner,
  [Parameter(Mandatory=$true)][string]$RepoName,
  [switch]$Private,
  [switch]$PushTags,
  [switch]$AllTags,
  [switch]$NoCreate
)

$ErrorActionPreference = 'Stop'

function Write-Info($msg){ Write-Host "[INFO] $msg" -ForegroundColor Cyan }
function Write-Ok($msg){ Write-Host "[ OK ] $msg" -ForegroundColor Green }
function Write-Warn($msg){ Write-Host "[WARN] $msg" -ForegroundColor Yellow }
function Write-Err($msg){ Write-Host "[ERR ] $msg" -ForegroundColor Red }

# Ensure TLS 1.2 for GitHub API
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

# 1) Basic checks
Write-Info "Checking Git installation and repo state..."
& git --version | Out-Null

$inside = (& git rev-parse --is-inside-work-tree) 2>$null
if ($LASTEXITCODE -ne 0 -or "$inside" -ne "true") {
  Write-Err "This directory is not a Git repository. Run 'git init' first."
  exit 1
}

# Determine current branch
$currentBranch = (& git rev-parse --abbrev-ref HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($currentBranch)) {
  Write-Err "Unable to determine current branch."
  exit 1
}
Write-Info "Current branch: $currentBranch"

# 2) Build target URLs
$remoteUrl = "https://github.com/$Owner/$RepoName.git"
$apiRepoUrl = "https://api.github.com/repos/$Owner/$RepoName"

# 3) Ensure origin points to the right place (create or replace)
$hasOrigin = $true
try {
  $existingOrigin = (& git remote get-url origin).Trim()
} catch {
  $hasOrigin = $false
}

if ($hasOrigin) {
  if ($existingOrigin -ne $remoteUrl) {
    Write-Warn "origin remote exists but points to:`n  $existingOrigin`nReplacing with:$remoteUrl"
    & git remote set-url origin $remoteUrl
    Write-Ok "origin updated."
  } else {
    Write-Ok "origin already set correctly."
  }
} else {
  Write-Info "Adding origin => $remoteUrl"
  & git remote add origin $remoteUrl
  Write-Ok "origin added."
}

# 4) Optionally create the repo on GitHub (if missing)
$needCreate = -not $NoCreate
$repoExists = $false
try {
  $resp = Invoke-WebRequest -Method GET -Uri $apiRepoUrl -Headers @{ "User-Agent" = "githelpme.ps1" } -UseBasicParsing
  if ($resp.StatusCode -eq 200) { $repoExists = $true }
} catch {
  # 404 means "not found" -> we will create if allowed
  if ($_.Exception.Response -and ($_.Exception.Response.StatusCode.value__ -ne 404)) {
    Write-Warn "GitHub GET check returned: $($_.Exception.Message)"
  }
}

if (-not $repoExists -and $needCreate) {
  Write-Info "GitHub repo not found: $Owner/$RepoName"
  Write-Info "Creating repo via GitHub API..."

  # Ask for a PAT if we need to create
  Write-Host "Enter a GitHub Personal Access Token (classic) with 'repo' scope (input hidden):"
  $sec = Read-Host -AsSecureString
  $ptr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($sec)
  try {
    $token = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($ptr)
  } finally {
    [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($ptr)
  }

  if ([string]::IsNullOrWhiteSpace($token)) {
    Write-Err "No token provided; cannot create the repository automatically."
    Write-Info "Create it manually at https://github.com/new then re-run without -NoCreate."
    exit 1
  }

  $createBody = @{
    name   = $RepoName
    @private = [bool]$Private
  } | ConvertTo-Json

  try {
    $createResp = Invoke-WebRequest -Method POST -Uri "https://api.github.com/user/repos" `
      -Headers @{ 
        Authorization = "token $token"
        "User-Agent"  = "githelpme.ps1"
        Accept        = "application/vnd.github+json"
      } -ContentType "application/json" -Body $createBody -UseBasicParsing

    if ($createResp.StatusCode -in 200,201) {
      Write-Ok "GitHub repository created: $Owner/$RepoName"
    } else {
      Write-Warn "Unexpected status code creating repo: $($createResp.StatusCode)"
    }
  } catch {
    # 422 -> already exists or name taken (possibly under org), treat as exists
    $status = $_.Exception.Response.StatusCode.value__
    if ($status -eq 422) {
      Write-Warn "GitHub reports the repository may already exist (HTTP 422). Proceeding."
    } else {
      Write-Err "Failed to create repo via API: $($_.Exception.Message)"
      Write-Info "You can create it manually and re-run."
      exit 1
    }
  }
} elseif ($repoExists) {
  Write-Ok "GitHub repo exists."
} else {
  Write-Warn "Skipping repo creation (-NoCreate). Make sure it exists at $remoteUrl"
}

# 5) Ensure at least one commit exists (GitHub refuses empty pushes)
try {
  & git rev-parse --verify HEAD | Out-Null
} catch {
  Write-Info "No commits found. Creating initial commit..."
  & git add -A
  & git commit -m "Initial commit"
  Write-Ok "Initial commit created."
}

# 6) Push current branch
Write-Info "Pushing branch '$currentBranch' to origin..."
& git push -u origin $currentBranch
if ($LASTEXITCODE -ne 0) {
  Write-Err "Push failed. If you used HTTPS, Git may prompt for login or PAT. Try again after authenticating."
  Write-Info "Alternatively, configure a credential helper: 'git config --global credential.helper manager-core'"
  exit 1
}
Write-Ok "Branch pushed."

# 7) Optional tag pushes
if ($AllTags) { $PushTags = $true }

if ($PushTags) {
  # push alpha-v3 if present
  $alphaTag = (& git tag -l "alpha-v3").Trim()
  if ($alphaTag -eq "alpha-v3") {
    Write-Info "Pushing tag 'alpha-v3'..."
    & git push origin alpha-v3
    if ($LASTEXITCODE -eq 0) { Write-Ok "alpha-v3 pushed." } else { Write-Warn "Failed to push alpha-v3." }
  } else {
    Write-Warn "Tag 'alpha-v3' not found locally."
  }

  if ($AllTags) {
    Write-Info "Pushing ALL tags..."
    & git push --tags
    if ($LASTEXITCODE -eq 0) { Write-Ok "All tags pushed." } else { Write-Warn "Failed to push some tags." }
  }
}

Write-Ok "Done. Remote is set to: $remoteUrl"
