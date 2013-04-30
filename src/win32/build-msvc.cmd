@echo off
setlocal
PATH ..\..\..\depkgs-msvc\tools;%PATH%
for /f %%i in ( 'sed -ne "s/.*[ \t]VERSION[ \t][ \t]*\x22\(.*\)\x22/\1/p" ^< ..\version.h' ) do @set VERSION=%%i 
echo Building Bacula version %VERSION%
vcbuild bacula.sln %*
endlocal
