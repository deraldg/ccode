@echo off
set EXE=build\Release\dottalkpp.exe
set TESTDIR=tests
set OUTDIR=results\raw
if not exist "%OUTDIR%" mkdir "%OUTDIR%"

for %%F in ("%TESTDIR%\*.txt") do (
  echo Running %%~nF...
  pushd data
  "%~dp0%EXE%" < "%%~fF" > "%~dp0%OUTDIR%\%%~nF.out" 2> "%~dp0%OUTDIR%\%%~nF.err"
  popd
)
echo Done.
