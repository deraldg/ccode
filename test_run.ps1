$root  = Split-Path -Parent $PSScriptRoot
$exe   = Join-Path $root 'build\Release\dottalkpp.exe'
$test1 = Join-Path $root 'tests\delete_blank_recall.txt'

$cmds = @"
USE students
TEST "$test1" VERBOSE
QUIT
"@

$cmds | & $exe
