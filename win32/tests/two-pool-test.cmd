REM
REM This is Arno's test.  It uses two pools, two tapes, and
REM  an autochanger. Note, the Director has three Pools in its
REM  conf:  Default, Full, and Inc.  Default is used in the
REM  NightlySave job by default.  What is backed up is what
REM  is in ${cwd}/tmp/file-list, which is by default the Bareos
REM  source code (i.e. the build directory).
REM
REM  Note, we use the virtual disk autochanger.
REM
SET TestName=two-pool-test
SET JobName=Two-pool

CALL scripts\functions set_debug 0

CALL config_out

CALL scripts\cleanup
CALL scripts\copy-2disk-confs >nul 2>&1
CALL scripts\prepare-two-disks

REM Make a relatively large backup set 5 x source code directory
REM  Reduced to 1 for portable
ECHO %CD:\=/%/build >tmp\file-list

CALL scripts\functions start_test

REM Write out bconsole commands to a file
sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\two-pool-test.bscr >tmp\bconcmds

CALL scripts\functions run_bareos
CALL scripts\functions check_for_zombie_jobs storage=DDS-4
CALL scripts\functions stop_bareos

CALL scripts\functions check_two_logs
CALL scripts\functions check_restore_diff
CALL scripts\functions end_test
