REM
REM Run a simple backup of the Bareos build directory using the compressed option
REM   then restore a few selected files.
REM
SETLOCAL ENABLEDELAYEDEXPANSION
SET TestName=restore2-by-file-test
SET JobName=restore2byfile

CALL scripts\functions set_debug 0
CALL scripts\functions copy_test_confs

ECHO %CD:\=/%/build >tmp\file-list
sed -e "s;^;%CD:\=/%;" scripts\flist >tmp\restore2-list

CALL scripts\functions change_jobname CompressedTest %JobName%
CALL scripts\functions start_test

sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\restore2-by-file-test.bscr >tmp\bconcmds

CALL scripts\functions run_bareos
CALL scripts\functions check_for_zombie_jobs storage=File
CALL scripts\functions stop_bareos

CALL scripts\functions check_two_logs
SET dstat=0
FOR /f %%i IN ( tmp\restore2-list ) DO (
   SET SUBDIR=%%i
   diff %%i tmp/bareos-restores/!SUBDIR::=!
   IF %ERRORLEVEL% NEQ 0 SET dstat=1
)
CALL scripts\functions end_test
