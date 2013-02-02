REM
REM Run a simple backup of the Bareos build directory using the compressed option
REM   then restore it.
REM

SET TestName=restore-by-file-test
SET JobName=restorebyfile

CALL scripts\functions set_debug 0
CALL scripts\functions copy_test_confs

ECHO %CD:\=/%/tmp/build >tmp\file-list
MKDIR tmp\build
COPY build\src\dird\*.c tmp\build >nul 2>&1
DIR /b tmp\build >tmp\1
sed -e "s;^;%CD:\=/%/tmp/build/;" tmp\1 >tmp\restore-list
DEL tmp\1

CALL scripts\functions change_jobname CompressedTest %JobName%
CALL scripts\functions start_test

sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\restore-by-file-test.bscr >tmp\bconcmds

CALL scripts\functions run_bareos
CALL scripts\functions check_for_zombie_jobs storage=File
CALL scripts\functions stop_bareos

CALL scripts\functions check_two_logs
CALL scripts\functions check_restore_tmp_build_diff
CALL scripts\functions end_test
