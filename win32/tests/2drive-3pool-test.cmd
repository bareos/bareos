REM Run a simple backup of the Bareos build directory. Create three
REM   tapes, each in a different pool, then run some jobs asking for the
REM   volume on drive three.  It should find it without moving the
REM   volume.
REM
REM This script uses the virtual disk autochanger and two drives
REM
SET TestName="2drive-3pool-disk"
SET JobName="2dr3pooldisk"

CALL scripts\functions set_debug 0

CALL scripts\cleanup
CALL scripts\copy-2disk-drive-confs
CALL scripts\prepare-two-disks

ECHO %CD:\=/%/build >tmp\file-list
CALL scripts\functions change_jobname NightlySave %JobName%
CALL scripts\functions start_test

REM Turn off Prefer Mounted Volumes so we use 2 drives
REM outf="tmp/sed_tmp"
REM  ECHO "s%REM Prefer Mounted Volumes%  Prefer Mounted Volumes%g" >${outf}
REM cp bin/bareos-dir.conf tmp/1
REM Comment the next line out to write everything to one drive
REM  otherwise, it writes the two jobs to different drives
REM sed -f ${outf} tmp/1 >bin/bareos-dir.conf

REM Write out bconsole commands
sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\2drive-3pool-test.1.bscr >tmp\bconcmds
CALL scripts\functions run_bareos

sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\2drive-3pool-test.2.bscr >tmp\bconcmds
CALL scripts\functions run_bconsole

CALL scripts\functions check_for_zombie_jobs storage=DDS-4
CALL scripts\functions stop_bareos

CALL scripts\functions check_two_logs
CALL scripts\functions check_restore_diff

CALL scripts\functions end_test
