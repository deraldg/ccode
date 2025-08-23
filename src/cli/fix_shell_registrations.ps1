param(
  [string]$File = "shell.cpp",
  [switch]$DryRun
)

# Resolve target file
if (!(Test-Path $File)) {
  throw "File not found: $File (run this in the folder containing shell.cpp or pass -File <path>)"
}
$path = (Resolve-Path $File).Path

# Read file
$content = Get-Content $path -Raw

# Match lines like:
# cli::reg.add("DELETE", [](DbArea& A, std::istringstream& S){ cmd_DELETE(A,S); });
# (Allows any params inside (), optional cli:: prefix on the function, any spacing/newlines)
$regexOptions = [System.Text.RegularExpressions.RegexOptions]::Singleline
$pattern = 'cli::reg\.add\(\s*"([^"]+)"\s*,\s*\[\s*\]\s*\(\s*[^)]*\)\s*\{\s*((?:cli::)?cmd_[A-Za-z0-9_]+)\s*\(\s*[^)]*\)\s*;\s*\}\s*\)\s*\);'

$matches = [System.Text.RegularExpressions.Regex]::Matches($content, $pattern, $regexOptions)

if ($DryRun) {
  Write-Host "Would replace $($matches.Count) registration(s):"
  $i = 1
  foreach ($m in $matches) {
    $name = $m.Groups[1].Value
    $fn   = $m.Groups[2].Value
    "{0,2}. {1}  ->  {2}" -f $i, $name, $fn | Write-Host
    $i++
  }
  return
}

if ($matches.Count -eq 0) {
  Write-Host "No lambda-style registrations matched in $path."
  return
}

# Replace with function-pointer style: cli::reg.add("NAME", cmd_NAME);
$replacement = 'cli::reg.add("$1", $2);'
$newContent  = [System.Text.RegularExpressions.Regex]::Replace($content, $pattern, $replacement, $regexOptions)

# Backup and write
Copy-Item $path "$path.bak" -Force
Set-Content -Path $path -Value $newContent -NoNewline
Write-Host "Updated $path. Replaced $($matches.Count) registration(s). Backup written to $path.bak"
