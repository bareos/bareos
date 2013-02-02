REM
REM Run a simple backup of the Bareos build directory
REM  to two tapes where the maximum tape file size is set to 1M
REM  Note, this test simulates the tape filling and writing to
REM   the next tape. One minor wrinkle: we create the two tapes
REM   in the Scratch pool, so they should be switched to the Default
REM   Pool automatically.
REM We set TestVolume001 to not in the changer, so the algorithm
REM   should find TestVolume002 and use it rather than blocking.
REM
REM  Note we use the viritual disk autochanger
REM
SET TestName=scratch-pool-test
SET JobName=scratch-pool

CALL scripts\functions set_debug 0

CALL config_out
IF "%AUTOCHANGER%" == "nul" (
   ECHO two-volume-test skipped. No autochanger.
   EXIT /b 1
)

CALL scripts\cleanup
CALL scripts\copy-2disk-confs >nul 2>&1
CALL scripts\prepare-two-disks

ECHO %CD:\=/%/build >tmp\file-list

COPY bin\bareos-sd.conf tmp\1 >nul 2>&1
sed -e "s;# Maximum File Size;  Maximum File Size;g" tmp\1 >bin\bareos-sd.conf

CALL scripts\functions change_jobname NightlySave %JobName%
CALL scripts\functions start_test

REM Write out bconsole commands
sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\scratch-pool-test.bscr >tmp\bconcmds

CALL scripts\functions run_bareos
CALL scripts\functions check_for_zombie_jobs storage=DDS-4
CALL scripts\functions stop_bareos

CALL scripts\functions check_two_logs
CALL scripts\functions check_restore_diff
CALL scripts\functions end_test
