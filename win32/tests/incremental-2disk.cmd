REM
REM Run a simple backup of the Bareos build directory then create some
REM   new files, do an Incremental and restore those two files.
REM
REM This script uses the virtual disk autochanger
REM
SET TestName=incremental-2disk
SET JobName=Inc2disk

CALL scripts\functions set_debug 0

CALL config_out

CALL scripts\functions stop_bareos

CALL bin\drop_bareos_tables >nul 2>&1
CALL bin\make_bareos_tables >nul 2>&1
CALL bin\grant_bareos_privileges >nul 2>&1

CALL scripts\copy-2disk-confs >nul 2>&1
CALL scripts\prepare-two-disks

ECHO %CD:\=/%/tmp/build >tmp\file-list
IF NOT EXIST tmp\build MKDIR tmp\build

COPY build\src\dird\*.c tmp\build >nul 2>&1
ECHO %CD:\=/%/tmp/build/ficheriro1.txt>tmp\restore-list
ECHO %CD:\=/%/tmp/build/ficheriro2.txt>>tmp\restore-list

CALL scripts\functions change_jobname %JobName%
CALL scripts\functions start_test

REM Write out bconsole commands
sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\incremental-2disk.1.bscr >tmp\bconcmds

CALL scripts\functions run_bareos

ECHO ficheriro1.txt >tmp\build\ficheriro1.txt
ECHO ficheriro2.txt >tmp\build\ficheriro2.txt


sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\incremental-2disk.2.bscr >tmp\bconcmds

CALL scripts\functions run_bconsole

CALL scripts\functions check_for_zombie_jobs storage=File
CALL scripts\functions stop_bareos
REM
REM Delete .c files because we will only restored the txt files
REM
DEL tmp\build\*.c

CALL scripts\functions check_two_logs
CALL scripts\functions check_restore_tmp_build_diff
CALL scripts\functions end_test
