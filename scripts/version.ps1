param(
  [Parameter(Mandatory=$true)][string]$Version,  # e.g. 0.2.0
  [string]$Message = "Release $Version"
)

$ErrorActionPreference = "Stop"

git add -A
# If nothing to commit, continue
try { git commit -m $Message } catch { Write-Host "[*] No changes to commit." }

$tag = "v$Version"
# Recreate tag if it exists locally
if ((git tag --list $tag) -ne $null) {
  git tag -d $tag | Out-Null
}
git tag -a $tag -m $Message
git push
git push --tags

Write-Host "[OK] Tagged and pushed $tag" -ForegroundColor Green
