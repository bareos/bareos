REM
REM Run a backup of the Bareos build directory on two Volumes
REM   then migrate it to another device.
REM
REM This script uses the virtual disk autochanger
REM
SET TestName=migration-volume-test
SET JobName=MigVolBackup

CALL scripts\functions set_debug 0

CALL config_out

CALL scripts\cleanup
CALL scripts\copy-migration-confs
CALL scripts\prepare-two-disks

ECHO %CD:\=/%/build >tmp\file-list

REM COPY bin\bareos-sd.conf tmp\1
REM sed "s;# Maximum File Size;  Maximum File Size;" tmp\1 >bin\bareos-sd.conf

CALL scripts\functions change_jobname NightlySave %JobName%
CALL scripts\functions start_test

REM
REM Note, we first backup into Pool Default,
REM          then Migrate into Pool Full.
REM              Pool Default uses Storage=File
REM              Pool Full    uses Storage=DiskChanger

sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\migration-volume-test.bscr >tmp\bconcmds

CALL scripts\functions run_bareos
CALL scripts\functions check_for_zombie_jobs storage=File
CALL scripts\functions stop_bareos

CALL scripts\functions check_two_logs
CALL scripts\functions check_restore_diff
CALL scripts\functions end_test
