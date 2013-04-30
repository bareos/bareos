@ECHO off
setlocal ENABLEDELAYEDEXPANSION
REM
REM
REM Bacula interface to virtual autoloader using disk storage
REM
REM  $Id: disk-changer.in,v 1.3 2006/07/30 16:00:32 kerns Exp $
REM
REM  If you set in your Device resource
REM
REM  Changer Command = "disk-changer %c %o %S %a %d"
REM    you will have the following input to this script:
REM
REM  So Bacula will always call with all the following arguments, even though
REM    in some cases, not all are used.
REM
REM  disk-changer "changer-device" "command" "slot" "archive-device" "drive-index"
REM                      %1           %2       %3          %4             %5
REM
REM By default the autochanger has 10 Volumes and 1 Drive.
REM
REM Note: For this script to work, you *must" specify
REM    Device Type = File 
REM in each of the Devices associated with your AutoChanger resource.
REM
REM changer-device is the name of a file that overrides the default
REM   volumes and drives.  It may have:
REM       maxslot=n   where n is one based (default 10)
REM       maxdrive=m  where m is zero based (default 1 -- i.e. 2 drives)
REM  
REM   This code can also simulate barcodes. You simply put
REM   a list of the slots and barcodes in the "base" directory/barcodes.
REM   See below for the base directory definition.  Example of a 
REM   barcodes file:
REM      C:\TEMP\bacula\barcodes
REM      1:Vol001
REM      2:Vol002
REM      ...
REM 
REM archive-device is the name of the base directory where you want the
REM  Volumes stored appended with \drive0 for the first drive; \drive1
REM  for the second drive, ... For example, you might use
REM  C:\Temp\bacula\drive0  Note: you must not have a trailing slash, and
REM  the string (e.g. \drive0) must be unique, and it must not match
REM  any other part of the directory name. These restrictions could be
REM  easily removed by any clever script jockey.
REM
REM  Full example: disk-changer C:\Temp\bacula\conf load 1 C:\Temp\bacula\drive0 0
REM
REM The Volumes will be created with names slot1, slot2, slot3, ... maxslot in the
REM  base directory. In the above example the base directory is C:\Temp\bacula.
REM  However, as with tapes, their Bacula Volume names will be stored inside the
REM  Volume label. In addition to the Volumes (e.g. C:\Temp\bacula\slot1, 
REM  C:\Temp\bacula\slot3, ...) this script will create a C:\Temp\bacula\loadedn
REM  file to keep track of what Slot is loaded. You should not change this file.
REM

SET dbgfile=%CD%\disk-changer.log

REM to turn on logging, uncomment the following line
IF NOT EXIST %dbgfile% COPY nul %dbgfile% >nul

REM
REM check parameter count on commandline
REM
REM Check for special cases where only 2 arguments are needed, 
REM  all others are a minimum of 5
REM
IF "%1" EQU "" goto :param_count_invalid
IF "%2" EQU "" goto :param_count_invalid
IF "%2" EQU "list" goto :param_count_valid
IF "%2" EQU "slots" goto :param_count_valid
IF "%3" EQU "" goto :param_count_invalid
IF "%4" EQU "" goto :param_count_invalid
IF "%5" EQU "" goto :param_count_invalid
GOTO :param_count_valid

:param_count_invalid
   echo Insufficient number of arguments given.
   IF "%2" EQU "" (
      echo   At least two arguments must be specified.
   ) else echo   Command expected 5 arguments.
:usage
   ECHO.
   ECHO usage: disk-changer ctl-device command [slot archive-device drive-index]
   ECHO        Valid commands are: unload, load, list, loaded, and slots.
   EXIT /B 1

:param_count_valid

REM Setup arguments
SET ctl=%1
SET cmd=%2
SET slot=%3
SET device=%4
SET drive=%5

REM set defaults
SET maxdrive=1
SET maxslot=10

SET ctl=%ctl:/=\%
SET ctl=%ctl:\\=\%

SET device=%device:/=\%
SET device=%device:\\=\%

REM Pull in conf file
IF EXIST %ctl% CALL %ctl%

FOR %%i IN ( %ctl% ) DO SET dirname=%%~dpi
IF NOT EXIST %dirname%nul (
   ECHO ERROR: Autochanger directory "%dirname%" does not exist.
   ECHO        You must create it.
   EXIT /b 1
)

CALL :debug "Parms: %ctl% %cmd% %slot% %device% %drive%"
IF "%cmd%" EQU "unload" GOTO :cmdUnload
IF "%cmd%" EQU "load" GOTO :cmdLoad
IF "%cmd%" EQU "list" GOTO :cmdList
IF "%cmd%" EQU "loaded" GOTO :cmdLoaded
IF "%cmd%" EQU "slots" GOTO :cmdSlots
GOTO :cmdUnknown

:cmdUnload
   CALL :debug "Doing disk -f %ctl% unload %slot% %device% %drive%"
   IF NOT EXIST %dirname%loaded%drive% ECHO 0 >%dirname%loaded%drive%
   FOR /f %%i IN ( %dirname%loaded%drive% ) DO SET ld=%%i

   IF "%slot%" EQU "%ld%" ( 
      CALL :debug "Unloaded slot %ld% from drive %drive%"
      ECHO 0 >%dirname%loaded%drive%
      DEL %dirname%%device% >nul 2>nul
      SET rtn=0
   ) ELSE (
      ECHO Storage Element %slot% is Already Full
      CALL :debug "Storage Element %slot% is Already Full"
      SET rtn=1
   )
   GOTO :cmdExit

:cmdLoad
   CALL :debug "Doing disk -f %ctl% load %slot% %device% %drive%"
   SET ld=0
   IF NOT EXIST %dirname%loaded%drive% ECHO 0 >%dirname%loaded%drive%
   FOR /f %%i IN ( %dirname%loaded%drive% ) DO SET ld=%%i
   IF %ld% EQU 0 (
      IF NOT EXIST %dirname%slot%slot% COPY nul %dirname%slot%slot% >nul
      DEL %device%
      fsutil hardlink create %device% %dirname%slot%slot% >nul 2>&1
      SET rtn=%ERRORLEVEL%
      IF !rtn! EQU 0 (
         ECHO %slot% >%dirname%loaded%drive%
         CALL :debug "Loaded slot %slot% into drive %drive%"
      ) ELSE (
         CALL :debug "Create hardlink failed, return = !rtn!"
      )
   ) ELSE (
      ECHO Drive %drive% Full - Storage element %ld% loaded
      CALL :debug "Drive %drive% Full - Storage element %ld% loaded"
   )
   GOTO :cmdExit

:cmdList
   CALL :debug "Doing disk -f %ctl% -- to list volumes"
   IF EXIST %dirname%barcodes (
      TYPE %dirname%barcodes 
   ) ELSE (
      FOR /l %%i IN ( 1, 1, %maxslot% ) DO ECHO %%i:
   )
   SET rtn=0
   GOTO :cmdExit

:cmdLoaded
   CALL :debug "Doing disk -f %ctl% %drive% -- to find what is loaded"
   IF EXIST %dirname%loaded%drive% ( TYPE %dirname%loaded%drive% ) ELSE ECHO 0
   SET rtn=0
   GOTO :cmdExit

:cmdSlots
   CALL :debug "Doing disk -f %ctl% -- to get count of slots"
   ECHO %maxslot%
   SET rtn=0
   GOTO :cmdExit

:cmdExit
   EXIT /b %rtn%

:cmdUnknown
   ECHO '%cmd%' is an invalid command.
   GOTO :usage

REM
REM log whats done
REM
:debug
   IF NOT EXIST %dbgfile% GOTO :EOF
   FOR /f "usebackq tokens=2-4,5-7 delims=/:. " %%i IN ( '%DATE% %TIME%' ) do SET TIMESTAMP=%%k%%i%%j-%%l:%%m:%%n
   ECHO %TIMESTAMP% %* >> %dbgfile%
   GOTO :EOF
