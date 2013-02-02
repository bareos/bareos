REM
REM Run a simple backup of the Bareos build directory then create some
REM   new files, do an Incremental and restore those two files.
REM
REM This script uses the autochanger and two tapes
REM
SET TestName=incremental-2tape
SET JobName=inctwotape
CALL scripts\functions set_debug 0

CALL config_out
IF "%AUTOCHANGER%" == "nul" (
   ECHO incremental-2tape test skipped. No autochanger.
   EXIT
)

CALL scripts\functions stop_bareos
CALL drop_bareos_tables >nul 2>&1
CALL make_bareos_tables >nul 2>&1
CALL grant_bareos_privileges >nul 2>&1

CALL scripts\copy-2tape-confs
CALL scripts\cleanup-2tape
ECHO %CD:\=/%/tmp/build >tmp\file-list
IF NOT EXIST tmp\build MKDIR tmp\build
COPY build\src\dird\*.c tmp\build
ECHO %CD:\=/%/tmp/build/ficheriro1.txt>tmp\restore-list
ECHO %CD:\=/%/tmp/build/ficheriro2.txt>>tmp\restore-list

CALL scripts\functions start_test

sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\incremental-2tape.1.bscr >tmp\bconcmds

CALL scripts\functions run_bareos

ECHO ficheriro1.txt >tmp\build\ficheriro1.txt
ECHO ficheriro2.txt >tmp\build\ficheriro2.txt

sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\incremental-2tape.2.bscr >tmp\bconcmds
CALL scripts\functions run_bconsole
CALL scripts\bareos stop_bareos
CALL scripts\bareos check_two_logs
REM
REM Delete .c files because we will only restore the txt files
REM
DEL tmp\build\*.c
CALL scripts\bareos check_restore_tmp_build_diff
CALL scripts\bareos end_test
