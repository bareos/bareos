REM
REM Script to setup running Bareos regression tests
REM

IF "%1" == "" GOTO :usage
IF "%2" == "" GOTO :usage
IF "%3" == "" GOTO :usage
IF "%4" == "" GOTO :usage
IF "%5" == "" GOTO :usage
IF "%6" == "" GOTO :usage
IF "%7" == "" GOTO :usage
GOTO :param_count_ok

:usage
ECHO Incorrect number of arguments.
ECHO setup bareos-src build-type email-address smtp-host tape-device changer-device tape1-device
ECHO.
EXIT /b 1

:param_count_ok
RD /s /q build bin tmp working >nul 2>&1
MKDIR tmp

REM Copy new source
ECHO Copying source from %1
SET SRC=%1
FOR /r %1 %%i in ( debug release ) do IF EXIST %%i ECHO %%i | sed -e "s;%SRC:\=\\%\\;;" -e "/installer\\%2/d" -e "/win32\\%2/d" >>tmp\exclude_list
ECHO .bsc >>tmp\exclude_list
ECHO .ncb >>tmp\exclude_list

XCOPY %1 build\ /EXCLUDE:tmp\exclude_list /e /q

DEL build\src\win32\%2\winbareos*.exe

CALL scripts\install_bareos %2 %3 %4 %5 %6 %7

CALL scripts\bareos stop >nul 2>&1

IF NOT EXIST working MKDIR working
ECHO Running database creation scripts
CALL bin\create_bareos_database
CALL bin\drop_bareos_tables
CALL bin\make_bareos_tables
CALL bin\grant_bareos_privileges

REM Start and stop Bareos to ensure conf files are OK

CALL scripts\bareos start
CALL scripts\bareos stop >nul 2>&1

REM
REM Save Bareos default conf files for later use
REM
COPY bin\*.conf scripts >nul
GOTO :EOF
