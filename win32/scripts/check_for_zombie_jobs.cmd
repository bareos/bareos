REM
REM Check for zombie jobs (not terminated).
REM Also scan logs for ERROR messages

sed -e "s;@storage@;%1=%2;" scripts\check_for_zombie_jobs.bscr >tmp\bconcmds
CALL scripts\functions run_bconsole
grep "No Jobs running." tmp\dir.out >nul 2>&1
IF %ERRORLEVEL% NEQ 0 (
  ECHO.
  ECHO   !!!! Zombie Jobs in Director !!!!
  ECHO   !!!! Zombie Jobs in Director !!!! >>test.out
  ECHO.
)
grep "No Jobs running." tmp\fd.out >nul 2>&1
IF %ERRORLEVEL% NEQ 0 (
  ECHO.
  ECHO   !!!! Zombie Jobs in File daemon !!!!
  ECHO   !!!! Zombie Jobs in File daemon !!!! >>test.out
  ECHO.
)
grep "No Jobs running." tmp\sd.out >nul 2>&1
IF %ERRORLEVEL% NEQ 0 (
  ECHO.
  ECHO   !!!! Zombie Jobs in Storage daemon !!!!
  ECHO   !!!! Zombie Jobs in Storage daemon !!!! >>test.out
  ECHO.
)
grep "ERROR" tmp/log*.out >nul 2>&1
IF %ERRORLEVEL% EQU 0 (
  ECHO.
  ECHO   !!!! ERROR in log output !!!!
  ECHO   !!!! ERROR in log output !!!! >>test.out
  ECHO.
)
grep "Fatal Error" tmp\log*.out >nul 2>&1
IF %ERRORLEVEL% EQU 0 (
  ECHO.
  ECHO   !!!! Fatal Error in log output !!!!
  ECHO   !!!! Fatal Error in log output !!!! >>test.out
  ECHO.
)
