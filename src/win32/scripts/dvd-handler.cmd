@ECHO off
REM 
REM  Check the free space available on a writable DVD
REM  Should always exit with 0 status, otherwise it indicates a serious error.
REM  (wrong number of arguments, Python exception...)
REM 
REM   called:  dvd-handler <dvd-device-name> operation args
REM 
REM   operations used by Bacula:
REM 
REM    free   (no arguments)
REM       Scan the device and report the available space. It returns:
REM       Prints on the first output line the free space available in 
REM       bytes.
REM       If an error occurs, prints a negative number (-errno), followed,
REM       on the second line, by an error message.
REM 
REM    write  op filename
REM        Write a part file to disk.
REM        This operation needs two additional arguments.
REM        The first (op) indicates to
REM           0 -- append
REM           1 -- first write to a blank disk
REM           2 -- blank or truncate a disk
REM 
REM        The second is the filename to write
REM 
REM   operations available but not used by Bacula:
REM 
REM    test      Scan the device and report the information found.
REM              This operation needs no further arguments.
REM    prepare   Prepare a DVD+/-RW for being used by Bacula.
REM              Note: This is only useful if you already have some
REM              non-Bacula data on a medium, and you want to use
REM              it with Bacula. Don't run this on blank media, it
REM              is useless.
REM 
REM  
REM  $Id: dvd-handler.in,v 1.11 2006/08/30 16:19:30 kerns Exp $
REM 

setlocal ENABLEDELAYEDEXPANSION
SET PATH=%PATH%;@bin_dir_cmd@
REM  Configurable values:

SET dvdrwmediainfo=dvd+rw-mediainfo.exe
SET growcmd=growisofs.exe
SET dvdrwformat=dvd+rw-format.exe
SET dd=dd.exe
SET margin=10485760

REM Comment the following line if you want the tray to be reloaded
REM when writing ends.
SET growcmd=%growcmd% -use-the-force-luke^^^^^=notray

REM  end of configurable values

IF "%1" == "" GOTO :usage
IF "%2" == "" GOTO :usage

CALL :init %1

IF "%2" == "free" (
   CALL :free
   ECHO !ERRORLEVEL!
   ECHO No Error reported.
) ELSE IF "%2" == "prepare" (
   CALL :prepare
   ECHO Medium prepared successfully.
) ELSE IF "%2" == "test" (
   IF %freespace_collected% EQU 0 CALL :collect_freespace
   IF %mediumtype_collected% EQU 0 CALL :collect_mediumtype
   ECHO Class disk, initialized with device %device%
   ECHO  type = '!disktype!' mode='!diskmode!' status = '!diskstatus!'
   ECHO  next_session = !next_session! capacity = !capacity!
   ECHO  Hardware device is '!hardwaredevice!'
   ECHO  growcmd = '!growcmd!'
   ECHO  growparams = '!growparams!'
   ECHO.
   SET empty_disk=false
   CALL :is_blank
   IF !ERRORLEVEL! EQU 1 SET empty_disk=true
   SET rewritable=false
   CALL :is_RW
   IF !ERRORLEVEL! EQU 1 SET rewritable=true
   SET plus_RW_disk=false
   CALL :is_plus_RW
   IF !ERRORLEVEL! EQU 1 SET plus_RW_disk=true
   SET minus_RW_disk=false
   CALL :is_minus_RW
   IF !ERRORLEVEL! EQU 1 SET minus_RW_disk=true
   SET restricted_overwrite_disk=false
   CALL :is_restricted_overwrite
   IF !ERRORLEVEL! EQU 1 SET restricted_overwrite_disk=true
   SET blank_disk=false
   CALL :is_blank
   IF !ERRORLEVEL! EQU 1 SET blank_disk=true
   ECHO Empty disk: !empty_disk!  Blank Disk: !blank_disk!  ReWritable disk: !rewritable!
   ECHO Plus RW: !plus_RW_disk!  Minus RW: !minus_RW_disk!  Restricted Overwrite: !restricted_overwrite_disk!
   CALL :free
   ECHO Free space: !ERRORLEVEL!
) ELSE IF "%2" == "write" (
   IF "%3" == "" GOTO :usage
   IF "%4" == "" GOTO :usage
   CALL :write %3 %4
   ECHO Part file %4 successfully written to disk.
) ELSE (
   ECHO No operation - use test, free, prepare or write.
   ECHO THIS MIGHT BE A CASE OF DEBUGGING BACULA OR AN ERROR!
)
EXIT /b 0

REM ##########################################################################
REM
REM  The rest of this file is a set of subroutines that return DVD disk 
REM  information.
REM
REM  Status information about the device and the disk loaded is collected 
REM  only when asked for (for example dvd-freespace doesn't need to know the 
REM  media type, and dvd-writepart doesn't not always need to know the free 
REM  space).
REM
REM  The following subroutines are implemented:
REM  init       we need that...
REM  is_empty   Set ERRORLEVEL to TRUE if the disk is empty, blank... 
REM              this needs more work, especially concerning non-RW media 
REM              and blank vs. no filesystem considerations. Here, we 
REM              should also look for other filesystems - probably we don't
REM              want to silently overwrite UDF or ext2 or anything not 
REM              mentioned in fstab...
REM              (NB: I don't think it is a problem)
REM  is_RW       Set ERRORLEVEL to TRUE if the disk is RW (DVD-RW or DVD+RW)
REM  is_plus_RW  Set ERRORLEVEL to TRUE if the disk is DVD+RW
REM  is_minus_RW Set ERRORLEVEL to TRUE if the disk is DVD-RW
REM  is_blank    Set ERRORLEVEL to TRUE if the disk is blank
REM  free        Returns the available free space.
REM  write       Writes one part file to disk, either starting a new file
REM              system on disk, or appending to it.
REM              This method should also prepare a blank disk so that a
REM              certain part of the disk is used to allow detection of a
REM              used disk by all / more disk drives.
REM  blank       Blank the device
REM 
REM ##########################################################################

:init
SET device=%1
SET disktype=none
SET diskmode=none
SET diskstatus=none
SET hardwaredevice=none
SET pid=0
SET next_session=-1
SET capacity=-1

SET freespace_collected=0
SET mediumtype_collected=0

SET growcmd=%growcmd% -quiet -use-the-force-luke^^^=4gms

SET growparams=-A "Bacula Data" -input-charset=default -iso-level 3 -pad
SET growparams=%growparams% -p "dvd-handler / growisofs" -sysid "BACULADATA" -R
GOTO :EOF

:collect_freespace
SET next_session=0
SET capacity=0
FOR /f "delims== tokens=1*" %%i in ( '%growcmd% -F %device%' ) DO (
   IF "%%i" == "next_session" ( 
      SET next_session=%%j
   ) ELSE IF "%%i" == "capacity" (
      SET capacity=%%j
   ) ELSE IF "%%j" == "" (
      SET result=!result! %%i
   ) ELSE (
      SET RESULT=!result! %%i=%%j
   )
)
SET status=%ERRORLEVEL%
IF %STATUS% NEQ 0 (
   SET /a STATUS=STATUS ^& 0x7F
   IF !STATUS! EQU 112 (
      REM Kludge to force dvd-handler to return a free space of 0
      next_session = 1
      capacity = 1
      freespace_collected = 1
      GOTO :EOF
   ) ELSE (
      ECHO growisofs returned with an error !STATUS!. Please check you are using a patched version of dvd+rw-tools.
      EXIT !STATUS!
   )
)

IF %next_session% EQU 0 IF %capacity% EQU 0 (
   ECHO Cannot get next_session and capacity from growisofs.
   ECHO Returned: %result:|=^|%
   EXIT 1
)
SET freespace_collected=1
GOTO :EOF

:collect_mediumtype
SET hardwaredevice=
SET disktype=
SET diskmode=
SET diskstatus=
SET lasterror=
FOR /f "delims=: tokens=1,2 usebackq" %%i in ( `"%dvdrwmediainfo%" %device%` ) DO (
   IF "%%i" == "INQUIRY" FOR /f "tokens=*" %%k in ( "%%j" ) DO SET hardwaredevice=%%k
   IF "%%i" == " Mounted Media" FOR /f "tokens=1,2* delims=, " %%k in ( "%%j" ) DO (
      SET disktype=%%l
      SET diskmode=%%m
   )
   IF "%%i" == " Disc status" FOR /f "tokens=*" %%k in ( "%%j" ) DO SET diskstatus=%%k
)

IF NOT DEFINED disktype (
   ECHO Media type not found in %dvdrwmediainfo% output
   EXIT 1
)

IF "%disktype%" == "DVD-RW" IF NOT DEFINED diskmode (
   ECHO Media mode not found for DVD-RW in %dvdrwmediainfo% output
   EXIT 1
)

IF NOT DEFINED diskstatus (
   ECHO Disc status not found in %dvdrwmediainfo% output
   EXIT 1
)

SET mediumtype_collected=1
GOTO :EOF

:is_empty
IF %freespace_collected% EQU 0 CALL :collect_freespace
IF %next_session% EQU 0 ( EXIT /b 1 ) ELSE EXIT /b 0

:is_RW
IF %mediumtype_collected% EQU 0 CALL :collect_mediumtype
IF %disktype% == "DVD-RW" EXIT /b 1
IF %disktype% == "DVD+RW" EXIT /b 1
IF %disktype% == "DVD-RAM" EXIT /b 1
EXIT /b 0

:is_plus_RW
IF %mediumtype_collected% EQU 0 CALL :collect_mediumtype
IF "%disktype%" == "DVD+RW" EXIT /b 1
EXIT /b 0

:is_minus_RW
IF %mediumtype_collected% EQU 0 CALL :collect_mediumtype
IF "%disktype%" == "DVD-RW" EXIT /b 1
EXIT /b 0
      
:is_restricted_overwrite
IF %mediumtype_collected% EQU 0 CALL :collect_mediumtype
IF "%diskmode%" == "Restricted Overwrite" EXIT /b 1
EXIT /b 0

:is_blank
IF %mediumtype_collected% EQU 0 CALL :collect_mediumtype
IF "%diskstatus%" == "blank" EXIT /b 1
EXIT /b 0

:free
IF %freespace_collected% EQU 0 CALL :collect_freespace
FOR /f %%i in ( 'expr64 "capacity - next_session - margin"' ) DO SET fr=%%i
expr64 /q "fr < 0" && ( EXIT /b 0 ) || EXIT /b %fr%

REM %1 - newvol, %2 - partfile
:write
REM Blank DVD+RW when there is no data on it
CALL :is_plus_RW
SET tmpvar=%ERRORLEVEL%
CALL :is_blank
SET /a tmpvar=tmpvar + ERRORLEVEL
IF %1 EQU 1 IF %tmpvar% EQU 2 (
   ECHO DVD+RW looks brand-new, blank it to fix some DVD-writers bugs.
   CALL :blank
   ECHO Done, now writing the part file.
)
CALL :is_minus_RW
IF %ERRORLEVEL% NEQ 0 IF %1 NEQ 0 (
   CALL :is_restricted_overwrite
   IF !ERRORLEVEL! EQU 0 (
      ECHO DVD-RW is in %diskmode% mode, reformating it to Restricted Overwrite
      CALL :reformat_minus_RW
      ECHO Done, now writing the part file.
   )
)
if %1 NEQ 0 (
   REM Ignore any existing iso9660 filesystem - used for truncate
   if %1 EQU 2 (
      SET cmd_opts= -use-the-force-luke^^^=tty -Z
   ) ELSE (
      SET cmd_opts= -Z
   )
) ELSE (
   SET cmd_opts= -M
)
ECHO Running %growcmd% %growparams% %cmd_opts% %device% %2
%growcmd% %growparams% %cmd_opts% %device% %2
IF %ERRORLEVEL% NEQ 0 (
   ECHO Exited with status !ERRORLEVEL!
   EXIT !ERRORLEVEL!
)
GOTO :EOF

:prepare
CALL :is_RW
IF %ERRORLEVEL% EQU 0 (
   ECHO I won't prepare a non-rewritable medium
   EXIT /b 1
)

REM Blank DVD+RW when there is no data on it
CALL :is_plus_RW
SET result=%ERRORLEVEL%
CALL :is_blank
SET /a result=result + ERRORLEVEL
IF %result% EQU 2 (
   ECHO DVD+RW looks brand-new, blank it to fix some DVD-writers bugs.
   CALL :blank
   GOTO :EOF
)

CALL :is_minus_RW
IF %ERRORLEVEL% EQU 1 (
   CALL :is_restricted_overwrite
   IF !ERRORLEVEL! EQU 0 (
      ECHO DVD-RW is in %diskmode% mode, reformating it to Restricted Overwrite
      CALL :reformat_minus_RW
      GOTO :EOF
   )
)
CALL :blank
GOTO :EOF

:blank
ECHO Running %growcmd% -Z %device% =/dev/zero
%growcmd% -Z %device% =/dev/zero
IF %ERRORLEVEL% NEQ 0 (
   ECHO Exited with status !ERRORLEVEL!
   EXIT !ERRORLEVEL!
)
GOTO :EOF

:reformat_minus_RW
ECHO Running %dvdrwformat% -force %device%
%dvdrwformat% -force %device%
IF %ERRORLEVEL% NEQ 0 (
   ECHO Exited with status !ERRORLEVEL!
   EXIT !ERRORLEVEL!
)
GOTO :EOF

REM  class disk ends here.

:usage
ECHO Wrong number of arguments.
ECHO.
ECHO Usage:
ECHO.
ECHO dvd-handler DVD-DRIVE test
ECHO dvd-handler DVD-DRIVE free
ECHO dvd-handler DVD-DRIVE write APPEND FILE
ECHO dvd-handler DVD-DRIVE blank
ECHO.
ECHO where DVD-DRIVE is the drive letter of the DVD burner like D:
ECHO.
ECHO Operations:
ECHO test      Scan the device and report the information found.
ECHO           This operation needs no further arguments.
ECHO free      Scan the device and report the available space.
ECHO write     Write a part file to disk.
ECHO           This operation needs two additional arguments.
ECHO           The first indicates to append (0), restart the
ECHO           disk (1) or restart existing disk (2). The second
ECHO           is the file to write.
ECHO prepare   Prepare a DVD+/-RW for being used by Bacula.
ECHO           Note: This is only useful if you already have some
ECHO           non-Bacula data on a medium, and you want to use
ECHO           it with Bacula. Don't run this on blank media, it
ECHO           is useless.

EXIT /b 1
