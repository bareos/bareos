REM
REM Create a 60MB file with random bytes. Back it up to 6 Volumes
REM   each constrained to 10MB using the automatic labeling feature.
REM
SET TestName=six-vol-test
SET JobName=SixVol

CALL scripts\functions set_debug 0

CALL scripts\cleanup
CALL scripts\copy-testa-confs >nul 2>&1
ECHO %CD:\=/%/tmp/largefile >tmp\file-list

ECHO Creating a 60MB file with bareos-dir data ...
dd if=bin/bareos-dir.exe of=%CD:\=/%/tmp/1 bs=1024 count=500 >nul 2>&1
COPY /b tmp\1+tmp\1+tmp\1+tmp\1+tmp\1 tmp\2 >nul 2>&1
DEL tmp\1
COPY /b tmp\2+tmp\2 tmp\3 >nul 2>&1
DEL tmp\2
COPY /b tmp\3+tmp\3+tmp\3+tmp\3+tmp\3+tmp\3 tmp\4 >nul 2>&1
COPY /b tmp\3+tmp\3+tmp\3+tmp\3+tmp\3+tmp\4 tmp\largefile >nul 2>&1
DEL tmp\3 tmp\4

CALL scripts\functions change_jobname MultiVol %JobName%
CALL scripts\functions start_test

sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\six-vol-test.bscr >tmp\bconcmds

CALL scripts\functions run_bareos
CALL scripts\functions check_for_zombie_jobs storage=File
CALL scripts\functions stop_bareos

CALL scripts\functions check_two_logs
diff tmp\largefile  tmp\bareos-restores\%CD::=%\tmp\largefile >nul 2>&1
SET dstat=%ERRORLEVEL%
CALL scripts\functions end_test
