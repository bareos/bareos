REM
REM Run a simple backup of the Bareos build directory then create some
REM   new files, do an Incremental and restore those two files.
REM
REM This script uses the virtual disk autochanger and two drives
REM
SET TestName=2drive-incremental-2disk
SET JobName=2drive2disk

CALL config_out
CALL scripts\functions set_debug 0

CALL scripts\cleanup
CALL scripts\copy-2disk-drive-confs >nul 2>&1
CALL scripts\prepare-two-disks

CALL scripts\functions change_jobname localhost-fd %JobName%
CALL scripts\functions start_test

ECHO %CD:\=/%/tmp/build >tmp\file-list
IF NOT EXIST tmp\build MKDIR tmp\build
COPY build\src\dird\*.c tmp\build >nul 2>&1

ECHO %CD:\=/%/tmp/build/ficheriro1.txt>tmp\restore-list
ECHO %CD:\=/%/tmp/build/ficheriro2.txt>>tmp\restore-list

REM Turn off Prefer Mounted Volumes so we use2 drives
COPY bin\bareos-dir.conf tmp\1 >nul 2>&1
REM Comment the next line out to write everything to one drive
REM  otherwise, it writes the two jobs to different drives
sed -e "s;# Prefer Mounted Volumes;  Prefer Mounted Volumes;g" tmp\1 >bin\bareos-dir.conf

REM Write out bconsole commands
sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\2drive-incremental-2disk.1.bscr >tmp\bconcmds

CALL scripts\functions run_bareos
ECHO ficheriro1.txt >tmp\build\ficheriro1.txt
ECHO ficheriro2.txt >tmp\build\ficheriro2.txt

sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\2drive-incremental-2disk.2.bscr >tmp\bconcmds

CALL scripts\functions run_bconsole
CALL scripts\functions check_for_zombie_jobs storage=DDS-4
CALL scripts\functions stop_bareos

CALL scripts\functions check_two_logs
REM The restore should read from TestVolume002, which was on drive 1
grep TestVolume002 tmp\log2.out >nul 2>&1
IF %ERRORLEVEL% NEQ 0 SET rstat=%ERRORLEVEL%

REM
REM Delete .c files because we will only restored the txt files
REM
DEL tmp\build\*.c
CALL scripts\functions check_restore_tmp_build_diff

CALL scripts\functions end_test
