REM
REM Run a simple backup of the Bareos build directory
REM   then restore it.
REM

SET TestName=backup-bareos-test
SET JobName=backup
CALL scripts\functions set_debug 0

CALL scripts\cleanup
CALL scripts\copy-confs >nul 2>&1

CALL scripts\functions change_jobname Client1 %JobName%
CALL scripts\functions start_test

sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\backup-bareos-test.bscr >tmp\bconcmds

CALL scripts\functions run_bareos
CALL scripts\functions check_for_zombie_jobs storage=File
CALL scripts\functions stop_bareos

CALL scripts\functions check_two_logs
CALL scripts\functions check_restore_diff
CALL scripts\functions end_test
