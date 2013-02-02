REM Run four jobs at the same time, with two Volumes.
REM   Set max Vol bytes of first volume
REM   to less than total backup to force Bareos to use the second
REM   Volume.
REM
SET TestName="2drive-concurrent-test"
SET JobName=Four-concurrent-jobs
CALL CALL scripts\functions set_debug 0

CALL scripts\cleanup
CALL scripts\copy-2disk-drive-confs
CALL scripts\prepare-two-disks
ECHO %CD:\=/%/build >tmp\file-list

CALL scripts\functions change_jobname NightlySave %JobName%
CALL scripts\functions start_test

sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\2drive-concurrent-test.bscr >tmp\bconcmds

CALL scripts\functions run_bareos
CALL scripts\functions check_for_zombie_jobs storage=DDS-4
CALL scripts\functions stop_bareos

CALL scripts\functions check_two_logs
CALL scripts\functions check_restore_diff
CALL scripts\functions end_test
