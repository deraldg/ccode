param(
    [string]$RootPath = 'C:\',
    [int]$TopN = 30
)

Write-Host "Scanning $RootPath ... this may take a while." -ForegroundColor Cyan

# Ensure we’re running with enough rights to see most folders
$principal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Warning "Not running as Administrator. Some protected folders may be skipped."
}

# Helper: safely get directory children, skipping inaccessible paths
function Get-SafeChildItems {
    param(
        [string]$Path
    )
    try {
        Get-ChildItem -LiteralPath $Path -Force -ErrorAction Stop
    }
    catch {
        Write-Verbose "Skipping (no access): $Path"
        @()
    }
}

# Helper: detect junctions / reparse points so we don’t loop
function Is-ReparsePoint {
    param(
        [System.IO.FileSystemInfo]$Item
    )
    return ($Item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0
}

# Walk the tree and accumulate sizes per top-level folder
$results = @()

$root = Get-Item -LiteralPath $RootPath -ErrorAction Stop

$topLevel = Get-SafeChildItems -Path $root.FullName | Where-Object { $_.PSIsContainer }

foreach ($folder in $topLevel) {
    Write-Host "Measuring: $($folder.FullName)" -ForegroundColor Yellow

    $totalBytes = 0

    # Stack-based traversal to avoid recursion depth issues
    $stack = New-Object System.Collections.Stack
    $stack.Push($folder)

    while ($stack.Count -gt 0) {
        $current = $stack.Pop()

        # Skip junctions / reparse points
        if (Is-ReparsePoint -Item $current) {
            Write-Verbose "Skipping junction: $($current.FullName)"
            continue
        }

        $children = Get-SafeChildItems -Path $current.FullName

        foreach ($child in $children) {
            if ($child.PSIsContainer) {
                $stack.Push($child)
            }
            else {
                $totalBytes += $child.Length
            }
        }
    }

    $results += [PSCustomObject]@{
        Path       = $folder.FullName
        SizeBytes  = $totalBytes
        SizeGB     = [math]::Round($totalBytes / 1GB, 3)
    }
}

$drive = Get-PSDrive -Name ($RootPath.Substring(0,1)) -ErrorAction SilentlyContinue

if ($drive) {
    Write-Host ""
    Write-Host "Drive summary for $($drive.Name):" -ForegroundColor Cyan
    Write-Host ("  Used:   {0:N2} GB" -f (($drive.Used) / 1GB))
    Write-Host ("  Free:   {0:N2} GB" -f (($drive.Free) / 1GB))
    Write-Host ("  Total:  {0:N2} GB" -f (($drive.Used + $drive.Free) / 1GB))
    Write-Host ""
}

$results =
    $results |
    Sort-Object -Property SizeBytes -Descending |
    Select-Object -First $TopN

Write-Host "Top $TopN folders under $RootPath by size:" -ForegroundColor Green
$results | Format-Table Path, SizeGB -AutoSize