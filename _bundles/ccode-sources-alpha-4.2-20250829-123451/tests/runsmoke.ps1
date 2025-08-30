# Runs the smoketests against your Release build.
# Usage (from repo root):  .\runsmoke.ps1
$exe = ".\build\Release\dottalkpp.exe"
$tests = @(
  "smoke_open_list.txt",
  "smoke_index.txt",
  "smoke_edit_pack.txt",
  "smoke_csv_min.txt",
  "smoke_foxhelp.txt"
)

foreach ($t in $tests) {
  Write-Host "`n=== RUNNING $t ===`n" -ForegroundColor Cyan
  Get-Content $t | & $exe
}
