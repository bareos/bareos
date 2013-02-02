REM
REM Run a simple backup of the Bareos build directory then migrate it
REM   to another device.
REM
REM This script uses the virtual disk autochanger
REM
SET TestName=migration-job-test
SET JobName=MigrationJobSave

CALL scripts\functions set_debug 0

CALL config_out

CALL scripts\cleanup
CALL scripts\copy-migration-confs
CALL scripts\prepare-two-disks
ECHO %CD:\=/%/build >tmp\file-list

CALL scripts\functions change_jobname NightlySave %JobName%
CALL scripts\functions start_test

REM
REM Note, we first backup into Pool Default,
REM          then Migrate into Pool Full.
REM              Pool Default uses Storage=File
REM              Pool Full    uses Storage=DiskChanger

REM Write out bconsole commands
sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\migration-job-test.bscr >tmp\bconcmds

CALL scripts\functions run_bareos
CALL scripts\functions check_for_zombie_jobs storage=File
CALL scripts\functions stop_bareos

CALL scripts\functions check_two_logs
CALL scripts\functions check_restore_diff
CALL scripts\functions end_test
