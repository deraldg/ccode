# Edit these two lines, then run this file in PowerShell.
$SourceDir = "C:\Users\deral\code\ccode\data"
$OutDir    = "C:\Users\deral\code\ccode\data_out"

$Py = (Get-Command py -ErrorAction SilentlyContinue) ?? (Get-Command python -ErrorAction SilentlyContinue)
if (-not $Py) { Write-Error "Python not found. Install Python 3.9+."; exit 1 }

& $Py.Source clone_dbf_with_memo.py --source "$SourceDir" --out "$OutDir" --verbose
