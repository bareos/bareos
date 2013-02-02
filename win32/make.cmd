@ECHO off
REM
REM Makefile for Bareos regression testing
REM
REM  Note, Makefile is built from Makefile.in, which you should not really
REM    need to change, by invoking:
REM
REM  config <user's configuration>
REM
REM e.g.
REM  config kern.conf
REM

SETLOCAL

IF EXIST config_var.cmd GOTO :already_configured
ECHO You must run config first and supply the name of your conf file.
ECHO Use prototype.conf as a template.
EXIT /B 1

:already_configured

SET PATH=%PATH%;%CD%\bin;%CD%\tools

REM suck in user's configuration
CALL config_var.cmd

IF EXIST "%BAREOS_SOURCE%\configure" GOTO :source_ok
ECHO Unable to access the Bareos source in %BAREOS_SOURCE%.  Update
ECHO BAREOS_SOURCE in your configuration file and rerun config.
EXIT /B 1

:source_ok
IF "%1" == "setup" GOTO :setup
IF "%1" == "bareos" GOTO :bareos
IF "%1" == "sed" GOTO :sed
IF "%1" == "test" GOTO :test
IF "%1" == "full_test" GOTO :full_test
IF "%1" == "root_test" GOTO :root_test
IF "%1" == "clean" GOTO :clean
IF "%1" == "reset" GOTO :reset
IF "%1" == "distclean" GOTO :distclean

IF EXIST "%1" (
   %1
) ELSE IF EXIST "%1.cmd" (
   %1
) ELSE IF EXIST tests\%1 (
   tests\%1
) ELSE IF EXIST tests\%1.cmd (
   tests\%1
) ELSE (
   ECHO Unsupported option: %1
   ECHO Valid options are:
   ECHO    setup, bareos, sed, test, full_test, root_test, clean, reset, distclean
   EXIT /b 1
)

:setup
CALL :bareos %2
CALL :sed
GOTO :EOF

:bareos
RD /s /q build bin weird-files tmp working 2>nul
MKDIR tmp working
SET BUILD_TYPE=%1
IF "%BUILD_TYPE%"=="" SET BUILD_TYPE=release
ECHO Doing: scripts\setup "%BAREOS_SOURCE%" %BUILD_TYPE% %EMAIL% %SMTP_HOST% %TAPE_DRIVE% %AUTOCHANGER% %AUTOCHANGER_PATH% %TAPE_DRIVE1%
CALL scripts\setup "%BAREOS_SOURCE%" %BUILD_TYPE% %EMAIL% %SMTP_HOST% %TAPE_DRIVE% %AUTOCHANGER% %AUTOCHANGER_PATH% %TAPE_DRIVE1%
GOTO :EOF

:sed
ECHO Doing: scripts\do_sed %EMAIL% %SMTP_HOST% %TAPE_DRIVE% %AUTOCHANGER% %AUTOCHANGER_PATH% %TAPE_DRIVE1%
CALL scripts\do_sed %EMAIL% %SMTP_HOST% %TAPE_DRIVE% %AUTOCHANGER% %AUTOCHANGER_PATH% %TAPE_DRIVE1%
GOTO :EOF

:test
REM Run all non-root userid tests
CALL all-non-root-tests
GOTO :EOF

:full_test
REM run all file and tape tests
CALL all-tape-and-file-tests
GOTO :EOF

:root_test
REM These tests require you to run as root
CALL all-root-tests
GOTO :EOF

:clean
CALL scripts\cleanup
DEL \tmp\file-list tmp\* working\* test.out diff >nul 2>&1
DEL 1 2 3 scripts\1 scripts\2 scripts\3 tests\1 tests\2 tests\3 >nul 2>&1
DEL .#* /s >nul 2>&1
GOTO :EOF

:reset
REM Reset our userid after running as root
CALL scripts\cleanup
DEL \tmp\file-list tmp\file-list
DEL tmp\* working\*
GOTO :EOF

:distclean
RD /s /q bin build weird-files weird-files weird-files2 tmp working 2>nul
DEL scripts\*.conf
CALL :clean
GOTO :EOF
