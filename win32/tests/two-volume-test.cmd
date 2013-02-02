REM
REM  Run a simple backup of the Bareos build directory
REM   to two tapes where the maximum tape file size is set to 1M
REM   Note, this test simulates the tape filling and writing to
REM    the next tape.
REM
REM   Note we use the viritual disk autochanger
REM
SET TestName=two-volume-test
SET JobName=TwoVolume
CALL scripts\functions set_debug 1
CALL config_out

CALL scripts\cleanup
CALL scripts\copy-2disk-confs >nul 2>&1
CALL scripts\prepare-two-disks

ECHO %CD:\=/%/build >tmp\file-list

COPY bin\bareos-sd.conf tmp\1
sed -e "s;# Maximum File Size;  Maximum File Size;g" tmp\1 >bin\bareos-sd.conf

CALL scripts\functions change_jobname NightlySave %JobName%
CALL scripts\functions start_test

REM  Write out bconsole commands
sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\two-volume-test.bscr >tmp\bconcmds

CALL scripts\functions run_bareos
CALL scripts\functions check_for_zombie_jobs storage=DDS-4
CALL scripts\functions stop_bareos

CALL scripts\functions check_two_logs
CALL scripts\functions check_restore_diff
CALL scripts\functions end_test
