REM
REM Run a simple backup of the Bareos build directory but
REM   split the archive into two volumes, then restore
REM   files on only one of the volumes and ensure that
REM   the other volume is not used.  I.e. bsr optimization
REM   works.
REM
SET TestName=bsr-opt-test
SET JobName=bsr-opt

CALL scripts\functions set_debug 0
CALL scripts\functions copy_test_confs

ECHO %CD:\=/%/build >tmp\file-list

CALL scripts\functions change_jobname NightlySave %JobName%
CALL scripts\functions start_test

sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\bsr-opt-test.bscr >tmp\bconcmds

CALL scripts\functions run_bareos
CALL scripts\functions check_for_zombie_jobs storage=File1
CALL scripts\functions stop_bareos
REM
REM This test is not really reliable. What we want to do is
REM   to select files on only one Volume, then insure here
REM   that only one Volume is chosen.
REM
grep TestVolume002 working\restore.bsr >nul 2>&1
SET bsrstat=%ERRORLEVEL%
CALL scripts\functions check_two_logs

diff -r build\src\cats tmp\bareos-restores\%CD::=%\build\src\cats >nul 2>&1
SET /A errcount = %bsrstat% + %bstat% + %rstat%
IF %errcount% NEQ 0 (
   ECHO.
   ECHO.
   ECHO   !!!!! bsr-opt-test Bareos source failed!!! !!!!!
   ECHO   !!!!! bsr-opt-test failed!!! !!!!!  >>test.out

   SET /A errcount = %bstat% + %rstat%
   IF %errcount% NEQ 0 (
      ECHO   !!!!! Bad Job termination status     !!!!!
      ECHO   !!!!! Bad Job termination status     !!!!!  >>test.out
   ) ELSE IF %bsrstat% NEQ 0 (
      ECHO   !!!!! Volume selection error         !!!!!
      ECHO   !!!!! Volume selection error         !!!!!  >>test.out
   ) ELSE (
      ECHO   !!!!! Restored files differ          !!!!!
      ECHO   !!!!! Restored files differ          !!!!!  >>test.out
   )
   ECHO.
) ELSE (
   ECHO   ===== bsr-opt-test Bareos source OK %TIME% =====
   ECHO   ===== bsr-opt-test OK %TIME% =====  >>test.out
   CALL scripts\cleanup
)
