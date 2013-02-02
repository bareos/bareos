REM
REM Run a simple backup of the Bareos build directory
REM  to two tapes where the maximum tape file size is set to 1M
REM  Note, this test simulates the tape filling and writing to
REM   the next tape.
REM
SET TestName=two-volume-tape
SET JobName=twovoltape
CALL scripts\functions set_debug 0

CALL config_out
IF "%AUTOCHANGER%" == "nul" (
   ECHO two-volume-tape test skipped. No autochanger.
   EXIT /b 1
)

CALL scripts\cleanup
CALL scripts\copy-2tape-confs
CALL scripts\prepare-two-tapes

ECHO %CD:\=/%/build >tmp\file-list

COPY bin\bareos-sd.conf tmp\1
sed -e "s%# Maximum File Size%  Maximum File Size%g" >bin\bareos-sd.conf

CALL scripts\functions change_jobname %JobName%
CALL scripts\functions start_test

REM Write out bconsole commands
sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\two-volume-tape.1.bscr >tmp\bconcmds

CALL scripts\functions run_bareos
CALL scripts\functions check_for_zombie_jobs storage=DDS-4
CALL scripts\functions stop_bareos

mt -f %TAPE_DRIVE% rewind
mtx -f %AUTOCHANGER% unload
bin\bsleep 15

sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\two-volume-tape.2.bscr >tmp\bconcmds

CALL scripts\functions run_bareos
CALL scripts\functions check_for_zombie_jobs storage=DDS-4
CALL scripts\functions stop_bareos

CALL scripts\functions check_two_logs
CALL scripts\functions check_restore_diff
CALL scripts\functions end_test
