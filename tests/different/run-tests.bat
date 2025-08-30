@echo off
set EXE=build\Release\dottalkpp.exe
set TESTDIR=tests
set OUTDIR=results\raw

if not exist "%OUTDIR%" mkdir "%OUTDIR%"

for %%F in ("%TESTDIR%\*.txt") do (
  set "NAME=%%~nF"
  echo Running %%~nF...
  "%EXE%" < "%%F" > "%OUTDIR%\%%~nF.out"
)
echo Done.
