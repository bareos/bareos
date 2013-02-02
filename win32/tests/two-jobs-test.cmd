REM
REM Run a simple backup of the Bareos build directory using the compressed option
REM   then backup a second time and finally restore it
REM
SET TestName=two-jobs-test
SET JobName=Two-Jobs

CALL scripts\functions set_debug 0
CALL scripts\functions copy_test_confs

ECHO %CD:\=/%/build >tmp\file-list

CALL scripts\functions change_jobname CompressedTest %JobName%
CALL scripts\functions start_test

sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\two-jobs-test.1.bscr >tmp\bconcmds

CALL scripts\functions run_bareos
CALL scripts\functions check_for_zombie_jobs storage=File

ECHO Backup 1 done
touch build\src\dird\*.c

REM
REM run a second job
REM
sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\two-jobs-test.2.bscr >tmp\bconcmds

CALL scripts\functions run_bconsole
CALL scripts\functions check_for_zombie_jobs storage=File
CALL scripts\functions stop_bareos

CALL scripts\functions check_two_logs
CALL scripts\functions check_restore_diff
CALL scripts\functions end_test
