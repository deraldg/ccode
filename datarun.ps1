$env:DOTTALK_SCRIPTS = "c:\code\ccode\dottalkpp\data\scripts"
$env:DOTTALK_TMP     = "c:\code\ccode\dottalkpp\data\tmp"
cd \code\ccode\dottalkpp
copy /code/ccode/build/src/release/dottalkpp.exe bin
copy /code/ccode/build/src/release/dottalkpp.exe c:\users\deral\onedrive
./datarun.ps1
cd ..
cd ..
