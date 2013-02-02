REM
REM Run a simple backup of the Bareos build directory then create some
REM   new files, do an Incremental and restore those two files.
REM
SET TestName=incremental-test
SET JobName=Incremental

CALL scripts\functions set_debug 0
CALL scripts\functions copy_test_confs

ECHO %CD:\=/%/tmp/build>tmp/file-list

MKDIR tmp\build
COPY build\src\dird\*.c tmp\build >nul 2>&1

ECHO %CD:\=/%/tmp/build/ficheriro1.txt>tmp\restore-list
ECHO %CD:\=/%/tmp/build/ficheriro2.txt>>tmp\restore-list

CALL scripts\functions change_jobname CompressedTest %JobName%
CALL scripts\functions start_test

sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\incremental-test.1.bscr >tmp\bconcmds

CALL scripts\functions run_bareos
CALL scripts\functions check_for_zombie_jobs storage=File
REM
REM Now create two new files to be restored later
REM
bin\bsleep 1
ECHO ficheriro1.txt >tmp\build\ficheriro1.txt
COPY tmp\build\dird.c tmp\build\ficheriro2.txt >nul 2>&1

sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\incremental-test.2.bscr >tmp\bconcmds

CALL scripts\functions run_bconsole

bin\bsleep 1
touch tmp\build\ficheriro1.txt
touch tmp\build\ficheriro2.txt

sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\incremental-test.3.bscr >tmp\bconcmds

CALL scripts\functions run_bconsole

bin\bsleep 1

COPY tmp\build\ficheriro2.txt tmp\build\1 >nul 2>&1
sed -e "s;a;b;g" tmp\build\1 >tmp\build\ficheriro2.txt
DEL tmp\build\1

sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\incremental-test.4.bscr >tmp\bconcmds

CALL scripts\functions run_bconsole

bin\bsleep 1
touch tmp\build\ficheriro1.txt
touch tmp\build\ficheriro2.txt
sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\incremental-test.3.bscr >tmp\bconcmds

CALL scripts\functions run_bconsole

bin\bsleep 1
touch tmp\build\ficheriro1.txt
touch tmp\build\ficheriro2.txt
sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\incremental-test.3.bscr >tmp\bconcmds

CALL scripts\functions run_bconsole

bin\bsleep 1
touch tmp\build\ficheriro1.txt
touch tmp\build\ficheriro2.txt
sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\incremental-test.3.bscr >tmp\bconcmds

CALL scripts\functions run_bconsole
bin\bsleep 1
touch tmp\build\ficheriro1.txt
touch tmp\build\ficheriro2.txt

sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\incremental-test.5.bscr >tmp\bconcmds

CALL scripts\functions run_bconsole
CALL scripts\functions check_for_zombie_jobs storage=File
CALL scripts\functions stop_bareos

CALL scripts\functions check_two_logs
REM
REM Delete .c files because we will only restored the txt files
REM
DEL tmp\build\*.c
CALL scripts\functions check_restore_tmp_build_diff
CALL scripts\functions end_test
