REM
REM Run two jobs at the same time
REM

SET TestName=concurrent-jobs-test
SET JobName=concurrent-jobs

CALL scripts\functions set_debug 0
CALL scripts\functions copy_test_confs

ECHO %CD:\=/%/tmp/largefile >tmp\file-list
ECHO Creating a 50MB file with bareos-dir data ...
dd if=bin/bareos-dir.exe of=%CD:\=/%/tmp/1 bs=1024 count=512 >nul 2>&1
COPY /b tmp\1+tmp\1+tmp\1+tmp\1 tmp\2 >nul 2>&1
DEL tmp\1
COPY /b tmp\2+tmp\2+tmp\2+tmp\2+tmp\2 tmp\3 >nul 2>&1
DEL tmp\2
COPY /b tmp\3+tmp\3+tmp\3+tmp\3+tmp\3 tmp\largefile >nul 2>&1
DEL tmp\3

ECHO largefile created

CALL scripts\functions change_jobname CompressedTest %JobName%
CALL scripts\functions start_test

sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\concurrent-jobs-test.bscr >tmp\bconcmds

CALL scripts\functions run_bareos
CALL scripts\functions check_for_zombie_jobs storage=File
CALL scripts\functions stop_bareos

CALL scripts\functions check_two_logs
diff tmp\largefile  tmp\bareos-restores\%CD::=%\tmp\largefile >nul 2>&1
SET dstat=%ERRORLEVEL%
CALL scripts\functions end_test
