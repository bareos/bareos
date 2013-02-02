REM
REM A set of useful functions to be sourced in each test
REM

SET routine=%1
SHIFT

GOTO %routine%

:start_test
   ECHO.
   ECHO.
   ECHO  === Starting %TestName% at %TIME% ===
   ECHO  === Starting %TestName% at %TIME% === >>working\log
   ECHO.
   GOTO :EOF

:set_debug
   SET debug=%1
   IF "%debug%" EQU 1 (
     SET out=tee
   ) ELSE (
     SET out=output
   )
   GOTO :EOF

:run_bareos
   IF %debug% EQU 1 (
      CALL scripts\bareos start
      bin\bconsole -c bin\bconsole.conf <tmp\bconcmds
   ) ELSE (
      CALL scripts\bareos start >nul 2>&1
      bin\bconsole -c bin\bconsole.conf <tmp\bconcmds >nul 2>&1
   )
   GOTO :EOF

:run_bconsole
   IF %debug% EQU 1 (
      bin\bconsole -c bin\bconsole.conf <tmp\bconcmds
   ) ELSE (
      bin\bconsole -c bin\bconsole.conf <tmp\bconcmds >nul 2>&1
   )
   GOTO :EOF

:run_btape
   IF %debug% EQU 1 (
      bin\btape -c bin\bareos-sd.conf DDS-4 <tmp\bconcmds | tee tmp\log1.out
   ) ELSE (
      bin\btape -c bin\bareos-sd.conf DDS-4 <tmp\bconcmds >tmp\log1.out 2>&1
   )
   GOTO :EOF

:run_bscan
   IF %debug% EQU 1 (
      bin\bscan %1 %2 %3 %4 %5 %6 %7 %8 %9 | tee tmp\log.out
   ) ELSE (
      bin\bscan %1 %2 %3 %4 %5 %6 %7 %8 %9 >nul 2>&1
   )
   GOTO :EOF

:stop_bareos
   CALL scripts\bareos stop >nul 2>&1
   GOTO :EOF

:check_for_zombie_jobs
   CALL scripts\check_for_zombie_jobs %1 %2
   GOTO :EOF

:change_jobname
   IF "%2" == "" (
      SET oldname=NightlySave
      SET newname=%1
   ) ELSE (
      SET oldname=%1
      SET newname=%2
   )
   IF EXIST bin\1 DEL /f bin\1
   REN bin\bareos-dir.conf 1
   sed -e "s;%oldname%;%newname%;g" bin\1 >bin\bareos-dir.conf
REM  ECHO Job %oldname% changed to %newname%
   GOTO :EOF

:check_two_logs
   grep "^  Termination: *Backup OK" tmp\log1.out >nul 2>&1
   SET bstat=%ERRORLEVEL%
   grep "^  Termination: *Restore OK" tmp\log2.out >nul 2>&1
   SET rstat=%ERRORLEVEL%
   GOTO :EOF

:check_restore_diff
   diff -r build tmp\bareos-restores\%CD::=%\build >nul 2>&1
   SET dstat=%ERRORLEVEL%
   GOTO :EOF

:check_restore_tmp_build_diff
   diff -r tmp\build tmp\bareos-restores\%CD::=%\tmp\build >nul 2>&1
   SET dstat=%ERRORLEVEL%
   GOTO :EOF

:end_test
   SET /a errcount=%bstat% + %rstat% + %dstat%
   IF %errcount% NEQ 0 (
      ECHO.
      ECHO.
      ECHO   !!!!! %TestName% Bareos source failed!!! !!!!!
      ECHO   !!!!! %TestName% failed!!! !!!!! >>test.out
      IF %dstat% NEQ 0 (
         ECHO   !!!!! Restored files differ          !!!!!
         ECHO   !!!!! Restored files differ          !!!!! >>test.out
      ) ELSE (
         ECHO   !!!!! Bad Job termination status     !!!!!
         ECHO   !!!!! Bad Job termination status     !!!!! >>test.out
      )
      ECHO.
   ) ELSE (
      ECHO   ===== %TestName% Bareos source OK %TIME% =====
      ECHO   ===== %TestName% OK %TIME% ===== >>test.out
      IF %debug% EQU 0 scripts\cleanup
   )
   SET errcount=
   GOTO :EOF

:copy_tape_confs
   CALL scripts\copy-tape-confs >nul 2>&1
   CALL scripts\cleanup-tape
   GOTO :EOF

:copy_test_confs
   CALL scripts\copy-test-confs >nul 2>&1
   CALL scripts\cleanup
   GOTO :EOF
