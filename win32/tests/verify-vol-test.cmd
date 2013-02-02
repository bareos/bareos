REM
REM Run a simple backup of the Bareos build directory
REM   then verify the catalog.
REM
SET TestName=verify-vol-test
SET JobName=VerifyVol

CALL scripts\functions set_debug 0
CALL scripts\functions copy_test_confs

ECHO %CD:\=/%/build >tmp\file-list

CALL scripts\functions change_jobname NightlySave %JobName%
CALL scripts\functions start_test

sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\verify-vol-test.bscr >tmp\bconcmds

CALL scripts\functions run_bareos

bin\bsleep 2
CALL scripts\functions check_for_zombie_jobs storage=File
CALL scripts\functions stop_bareos

grep "^  Termination: *Backup OK" tmp\log1.out 2>&1 >nul
SET bstat=%ERRORLEVEL%
grep "^  Termination: *Verify OK" tmp\original 2>&1 >nul
SET rstat=%ERRORLEVEL%
SET dstat=0
CALL scripts\functions end_test
