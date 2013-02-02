REM
REM  Test if Bareos can automatically create a Volume label.
REM

SET TestName=auto-label-test
SET JobName=AutoLabel

CALL scripts\functions set_debug 0
CALL scripts\functions copy_test_confs

ECHO %CD:\=/%/build >tmp\file-list

COPY bin\bareos-dir.conf tmp\1
sed -e "s;# Label Format;  Label Format;" tmp\1 >bin\bareos-dir.conf

CALL scripts\functions change_jobname CompressedTest %JobName%
CALL scripts\functions start_test

sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\auto-label-test.bscr >tmp\bconcmds

CALL scripts\functions run_bareos
CALL scripts\functions check_for_zombie_jobs storage=File
CALL scripts\functions stop_bareos

CALL scripts\functions check_two_logs
CALL scripts\functions check_restore_diff
CALL scripts\functions end_test
