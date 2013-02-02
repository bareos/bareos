REM Create a big Volume > 5 GB and backup to it to test disk
REM   seeking on big volumes.  We cheat and artifically grow
REM   the volume.
SET TestName="big-vol-test"
SET JobName=bigvol

CALL scripts\functions set_debug 0

CALL scripts\cleanup
CALL scripts\copy-test-confs
ECHO %CD:\=/%/build >tmp\file-list

CALL scripts\functions change_jobname CompressedTest %JobName%
CALL scripts\functions start_test

sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\big-vol-test.1.bscr >tmp\bconcmds

CALL scripts\functions run_bareos
CALL scripts\functions check_for_zombie_jobs storage=File

REM
REM Now increase the size of the Volume using gigaslam
REM
CD %CD%\tmp
REM make big file
SET size=5200000000
%CD%/build/src/tools/grow TestVolume001 %size%
IF NOT ERRORLEVEL 0 GOTO ERROR
CD %CD%

sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" -e "s;@size@;%size%;g" tests\big-vol-test.2.bscr >tmp\bconcmds

CALL scripts\functions run_bconsole
CALL scripts\functions check_for_zombie_jobs storage=File
CALL scripts\functions stop_bareos

CALL scripts\functions check_two_logs
CALL scripts\functions check_restore_diff
CALL scripts\functions end_test
REM Get rid of big files
DEL /F %CD%\tmp\TestVolume001

:ERROR
ECHO Execute of %CD%\build\src\tools\grow failed.
