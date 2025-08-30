$stdin = @"
USE students
FIND FIRST_NAME "Diana"
SEEK LAST_NAME "Miller"
QUIT
"@
$in = Join-Path $PSScriptRoot "stdin.txt"
$out = Join-Path $PSScriptRoot "stdout.txt"
$stdin | Set-Content -Encoding ascii $in

& "$PSScriptRoot\..\build\Release\dottalkpp.exe" < $in | Tee-Object $out
Write-Host "---- RESULT ----"
Get-Content $out
