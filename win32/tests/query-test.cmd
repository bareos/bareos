REM
REM Run a simple backup of the Bareos build directory then create some
REM   new files, do a Differental then a bunch of query commands
REM   and finally restore the two files.
REM
SET TestName=query-test
SET JobName=query

CALL scripts\functions set_debug 0
CALL scripts\functions copy_test_confs

ECHO %CD:\=/%/tmp/build >tmp\file-list
MKDIR tmp\build
COPY build\src\dird\*.c tmp\build >nul 2>&1
ECHO %CD:\=/%/tmp/build/ficheriro1.txt>tmp\restore-list
ECHO %CD:\=/%/tmp/build/ficheriro2.txt>>tmp\restore-list

CALL scripts\functions change_jobname CompressedTest %JobName%
CALL scripts\functions start_test

sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\query-test.1.bscr >tmp\bconcmds

CALL scripts\functions run_bareos
CALL scripts\functions check_for_zombie_jobs storage=File

ECHO ficheriro1.txt >tmp\build\ficheriro1.txt
ECHO ficheriro2.txt >tmp\build\ficheriro2.txt

sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\query-test.2.bscr >tmp\bconcmds

CALL scripts\functions run_bareos
CALL scripts\functions check_for_zombie_jobs storage=File

ECHO ficheriro2.txt >tmp\build\ficheriro2.txt
sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\query-test.3.bscr >tmp\bconcmds

CALL scripts\functions run_bareos
CALL scripts\functions check_for_zombie_jobs storage=File
CALL scripts\functions stop_bareos

REM
REM Delete .c files because we will only restored the txt files
REM
DEL tmp\build\*.c

CALL scripts\functions check_two_logs
CALL scripts\functions check_restore_tmp_build_diff
CALL scripts\functions end_test
