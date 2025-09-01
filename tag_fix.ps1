# PowerShell 5.1–compatible script to normalize SET ORDER/TAG command routing.
# - Adds an alias so "SET ORDER TO ..." uses the same handler as "SETORDER".
# - Light touch: no structural refactors, just safe text edits + rebuild.

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path

function Add-SetOrderAliasInRegistry {
    param([string]$Path)

    if (!(Test-Path $Path)) {
        Write-Warning "File not found: $Path"
        return
    }

    $lines = Get-Content -LiteralPath $Path
    # Already present?
    foreach ($ln in $lines) {
        if ($ln -match '"SET ORDER TO"' -and $ln -match 'cmd_SETORDER') {
            Write-Host "Alias already present in $Path"
            return
        }
    }

    # Find the existing registration line for SETORDER and clone it
    for ($i = 0; $i -lt $lines.Count; $i++) {
        $ln = $lines[$i]
        if ($ln -match 'cmd_SETORDER' -and $ln -match '"SETORDER"') {
            $indentMatch = [regex]::Match($ln, '^\s*')
            $indent = $indentMatch.Value
            $aliasLine = $ln -replace '"SETORDER"','"SET ORDER TO"'

            # Insert alias just below the original line
            $before = @()
            if ($i -gt 0) { $before = $lines[0..$i] }
            $afterIndex = $i + 1
            $after = @()
            if ($afterIndex -le ($lines.Count - 1)) { $after = $lines[$afterIndex..($lines.Count - 1)] }
            $new = $before + @($aliasLine) + $after

            Copy-Item -LiteralPath $Path -Destination "$Path.bak" -Force
            Set-Content -LiteralPath $Path -Value $new -Encoding UTF8
            Write-Host "Added alias registration in $Path:`n  $aliasLine"
            return
        }
    }

    Write-Warning "Could not find a registration line for SETORDER in $Path. No changes made."
}

function Update-HelpForSetOrder {
    param([string]$Path)

    if (!(Test-Path $Path)) {
        Write-Host "Help file not found (skipping): $Path"
        return
    }

    $text = Get-Content -LiteralPath $Path -Raw
    if ($text -match 'SET ORDER TO') {
        Write-Host "Help already mentions 'SET ORDER TO' in $Path"
        return
    }

    # Try to augment the existing SETORDER help line with a friendly alias mention.
    $new = $text
    $did = $false

    # Common pattern: show the SETORDER syntax; append our alias right after it.
    $new2 = $new -replace '(SETORDER[^\r\n]*?)\r?\n', '$1' + "`r`n  SET ORDER TO TAG <name> | <n> | 0`r`n"
    if ($new2 -ne $new) { $new = $new2; $did = $true }

    if ($did) {
        Copy-Item -LiteralPath $Path -Destination "$Path.bak" -Force
        Set-Content -LiteralPath $Path -Value $new -Encoding UTF8
        Write-Host "Updated help in $Path (added 'SET ORDER TO ...')."
    } else {
        Write-Host "No recognizable SETORDER help block found in $Path (skipping)."
    }
}

# ---- Apply fixes ----
$reg = Join-Path $root 'src\cli\command_registry.cpp'
Add-SetOrderAliasInRegistry -Path $reg

# Help files (best-effort; skip silently if layout differs)
$help1 = Join-Path $root 'src\cli\cmd_help.cpp'
$help2 = Join-Path $root 'src\cli\cmd_foxref.cpp'
Update-HelpForSetOrder -Path $help1
Update-HelpForSetOrder -Path $help2

# ---- Rebuild ----
Write-Host "`nBuilding (Release)..."
Push-Location $root
& cmake --build build --config Release
Pop-Location

Write-Host "`nDone. Test with:"
Write-Host "  .\datarun.ps1"
Write-Host "  USE STUDENTS"
Write-Host "  INDEX ON STUDENT_ID TAG ID"
Write-Host "  SET ORDER TO TAG ID"
Write-Host "  SEEK 100004"
