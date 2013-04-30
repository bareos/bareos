@echo off
REM
REM
REM Bacula interface to mtx autoloader
REM
REM  $Id: mtx-changer.cmd 3718 2006-12-01 08:45:40Z robertnelson $
REM
REM  If you set in your Device resource
REM
REM  Changer Command = "mtx-changer %c %o %S %a %d"
REM    you will have the following input to this script:
REM
REM  Bacula will always call with all the following arguments, even though
REM    in some cases, not all are used.
REM
REM  mtx-changer "changer-device" "command" "slot" "archive-device" "drive-index"
REM                     %1           %2       %3          %4             %5
REM
REM  for example:
REM
REM  mtx-changer Changer0 load 1 Tape0 0
REM 
REM  will request to load the first cartidge into drive 0, where
REM   the changer device is Changer0, and the read/write device
REM   is Tape0.
REM
REM  If you need to an offline, refer to the drive as %4
REM    e.g.   mt -f %4 offline
REM
REM  Many changers need an offline after the unload. Also many
REM   changers need a sleep 60 after the mtx load.
REM
REM  N.B. If you change the script, take care to return either 
REM   the mtx exit code or a 0. If the script exits with a non-zero
REM   exit code, Bacula will assume the request failed.
REM

SET MTX="@bin_dir_cmd@\mtx.exe"
SET MT="@bin_dir_cmd@\mt.exe"
SET working_dir=@working_dir_cmd@

SET dbgfile="%working_dir%\mtx.log"

REM to turn on logging, uncomment the following line
REM copy nul "%working_dir%\mtx.log"

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
   ECHO usage: mtx-changer ctl-device command [slot archive-device drive-index]
   ECHO        Valid commands are: unload, load, list, loaded, and slots.
   EXIT /B 1

:param_count_valid

REM Setup arguments
SET ctl=%1
SET cmd=%2
SET slot=%3
SET device=%4
SET drive=%5

CALL :debug "Parms: %ctl% %cmd% %slot% %device% %drive%"
IF "%cmd%" EQU "unload" GOTO :cmdUnload
IF "%cmd%" EQU "load" GOTO :cmdLoad
IF "%cmd%" EQU "list" GOTO :cmdList
IF "%cmd%" EQU "loaded" GOTO :cmdLoaded
IF "%cmd%" EQU "slots" GOTO :cmdSlots
GOTO :cmdUnknown

:cmdUnload
   CALL :debug "Doing mtx -f %ctl% unload %slot% %drive%"
   %MT% -f %device% eject
   %MTX% -f %ctl% unload %slot% %drive%
   SET rtn=%ERRORLEVEL%
   GOTO :cmdExit

:cmdLoad
   CALL :debug "Doing mtx -f %ctl% load %slot% %drive%"
   %MTX% -f %ctl% load %slot% %drive%
   SET rtn=%ERRORLEVEL%
   IF ERRORLEVEL 1 GOTO :cmdExit
REM %MT% -f %device% load
REM bsleep 5
   CALL :wait_for_drive %device%
   REM Force block size to 0 for variable
   %MT% -f %device% setblk 0
   GOTO :cmdExit

:cmdList
   CALL :debug "Doing mtx -f %ctl% -- to list volumes"
   CALL :make_temp_file
REM Enable the following if you are using barcodes and need an inventory
REM   %MTX% -f %ctl% inventory
   %MTX% -f %ctl% status >%TMPFILE%
   SET rtn=%ERRORLEVEL%
   IF ERRORLEVEL 1 GOTO :cmdExit
   FOR /F "usebackq tokens=3,6 delims==: " %%i in ( `findstr /R /C:" *Storage Element [0-9]*:.*Full" %TMPFILE%` ) do echo %%i:%%j
   FOR /F "usebackq tokens=7,10" %%i in ( `findstr /R /C:"^Data Transfer Element [0-9]*:Full (Storage Element [0-9]" %TMPFILE%` ) do echo %%i:%%j
   DEL /F %TMPFILE% >nul 2>&1
REM
REM If you have a VXA PacketLoader and the above does not work, try
REM  turning it off and enabling the following line.
REM   %MTX% -f %ctl% status | grep " *Storage Element [0-9]*:.*Full" | sed "s/*Storage Element //" | sed "s/Full :VolumeTag=//"

   GOTO :cmdExit

:cmdLoaded
   CALL :debug "Doing mtx -f %ctl% %drive% -- to find what is loaded"
   CALL :make_temp_file
   %MTX% -f %ctl% status >%TMPFILE%
   SET rtn=%ERRORLEVEL%
   IF ERRORLEVEL 1 GOTO :cmdExit
   FOR /F "usebackq tokens=7" %%i in ( `findstr /R /C:"^Data Transfer Element %drive%:Full" %TMPFILE%` ) do echo %%i
   findstr /R /C:"^Data Transfer Element %drive%:Empty" %TMPFILE% >nul && echo 0
   DEL /F %TMPFILE% >nul 2>&1
   GOTO :cmdExit

:cmdSlots
   CALL :debug "Doing mtx -f %ctl% -- to get count of slots"
   CALL :make_temp_file
   %MTX% -f %ctl% status >%TMPFILE%
   SET rtn=%ERRORLEVEL%
   IF ERRORLEVEL 1 GOTO :cmdExit
   FOR /F "usebackq tokens=5" %%i in ( `findstr /R /C:" *Storage Changer" %TMPFILE%` ) do echo %%i
   DEL /F %TMPFILE% >nul 2>&1
   GOTO :cmdExit

:cmdExit
   EXIT /B %rtn%

:cmdUnknown
   ECHO '%cmd%' is an invalid command.
   GOTO :usage

REM
REM log whats done
REM
:debug
   IF NOT EXIST %dbgfile% GOTO :EOF
   FOR /F "usebackq tokens=2-4,5-7 delims=/:. " %%i in ( '%DATE% %TIME%' ) do SET TIMESTAMP=%%k%%i%%j-%%l:%%m:%%n
   ECHO %TIMESTAMP% %*>> %dbgfile%
   GOTO :EOF

REM
REM Create a temporary file
REM
:make_temp_file
   SET TMPFILE="%working_dir%\mtx.tmp"
   IF EXIST %TMPFILE% (
      ECHO Temp file security problem on: %TMPFILE%
      EXIT /B 1
   )
   GOTO :EOF

REM
REM The purpose of this function to wait a maximum 
REM   time for the drive. It will return as soon as 
REM   the drive is ready, or after waiting a maximum 
REM   of 300 seconds.
REM
:wait_for_drive
   FOR /L %%i IN ( 1, 1, 300 ) DO (
      %MT% -f %1 status | findstr ONLINE >NUL 2>&1
      IF %ERRORLEVEL%==0 GOTO :EOF
      CALL :debug "Device %1 - not ready, retrying..."
      bsleep 1
   )
   CALL :debug "Device %1 - not ready, timed out..."
   GOTO :EOF
