REM
REM Run a simple backup of the Bareos build directory but
REM   split the archive into two volumes then bscan it
REM   into the catalog after the backup. It also to a limited
REM   extent tests the purge volume and delete volume commands.
REM

SET TestName=bscan-test
SET JobName=bscan

CALL scripts\functions set_debug 0
CALL scripts\functions copy_test_confs

ECHO %CD:\=/%/build >tmp\file-list

CALL scripts\functions change_jobname NightlySave %JobName%
CALL scripts\functions start_test

sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\bscan-test.1.bscr >tmp\bconcmds

CALL scripts\functions run_bareos
CALL scripts\functions check_for_zombie_jobs storage=File1
CALL scripts\functions stop_bareos

ECHO volume=TestVolume001^|TestVolume002 >tmp\bscan.bsr

IF %debug% EQU 1 (
   bin\bscan -w working -m -s -v -b tmp/bscan.bsr -c bin/bareos-sd.conf %CD:\=/%/tmp | tee tmp\log.out
) ELSE (
   bin\bscan -w working -m -s -v -b tmp/bscan.bsr -c bin/bareos-sd.conf %CD:\=/%/tmp >nul 2>&1
)

sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\bscan-test.2.bscr >tmp\bconcmds

REM now run restore
CALL scripts\functions run_bareos
CALL scripts\functions check_for_zombie_jobs storage=File1
CALL scripts\functions stop_bareos

CALL scripts\functions check_two_logs
CALL scripts\functions check_restore_diff
CALL scripts\functions end_test
