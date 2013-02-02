REM Run a simple backup of untitled15.jpg that fails to
REM  restore properly in bug REM763
REM
SET TestName="encrypt-bug-test"
SET JobName=Crypto-bug
CALL scripts\functions set_debug 0

CALL scripts\cleanup
CALL scripts\copy-crypto-confs
ECHO %CD%/encrypt-bug.jpg   >tmp/file-list
ECHO %CD%/encrypt-bug.jpg   >tmp/restore-list
ECHO %CD%/encrypt-bug.txt  >>tmp/file-list
ECHO %CD%/encrypt-bug.txt  >>tmp/restore-list
ECHO %CD%/encrypt-bug2.txt >>tmp/file-list
ECHO %CD%/encrypt-bug2.txt >>tmp/restore-list
SET files="encrypt-bug.jpg encrypt-bug.txt encrypt-bug2.txt"
mkdir -p %CD%\tmp\build
for i in %files% do XCOPY /k %CD%\encrypt-bug*.* %CD%\tmp\build

CALL scripts\functions change_jobname NightlySave @JobName@
CALL scripts\functions start_test

sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\encrypt-bug-test.bscr >tmp\bconcmds

CALL scripts\functions run_bareos
sleep 2
CALL scripts\functions check_for_zombie_jobs storage=File
CALL scripts\functions stop_bareos

CALL scripts\functions check_two_logs
diff %CD%\tmp\build %CD%\tmp\bareos-restores\%CD%
SET dstat=%ERRORLEVEL%
CALL scripts\functions end_test
