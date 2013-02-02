REM
REM Run a simple backup of the Bareos build directory using the compressed option
REM   then backup four times, each with incremental then finally restore.
REM   It should require at least 4 different bsrs.
REM
SET TestName=four-jobs-test
SET JobName=SpanVol

CALL scripts\functions set_debug 0
CALL scripts\functions copy_test_confs

ECHO %CD:\=/%/build >tmp/file-list

CALL scripts\functions change_jobname CompressedTest %JobName%
CALL scripts\functions start_test

sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\four-jobs-test.1.bscr >tmp\bconcmds

CALL scripts\functions run_bareos
CALL scripts\functions check_for_zombie_jobs storage=File

ECHO Backup 1 done

REM make some files for the incremental to pick up
touch %CD:\=/%/build/src/dird/*.c
touch %CD:\=/%/build/src/lib/*.c

REM
REM run a second job
REM
sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\four-jobs-test.2.bscr >tmp\bconcmds

CALL scripts\functions run_bconsole
CALL scripts\functions check_for_zombie_jobs storage=File

ECHO Backup 2 done

touch %CD:\=/%/build/src/dird/*.c
touch %CD:\=/%/build/src/lib/*.c

REM
REM run a third job
REM
sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\four-jobs-test.3.bscr >tmp\bconcmds

CALL scripts\functions run_bconsole
CALL scripts\functions check_for_zombie_jobs storage=File

ECHO Backup 3 done

REM make some files for the incremental to pick up
touch %CD:\=/%/build/src/lib/*.c

REM
REM run a fourth job
REM
sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\four-jobs-test.4.bscr >tmp\bconcmds

CALL scripts\functions run_bconsole
CALL scripts\functions check_for_zombie_jobs storage=File

ECHO Backup 4 done
REM
REM now do several restores to ensure we cleanup between jobs
REM
sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\four-jobs-test.5.bscr >tmp\bconcmds

CALL scripts\functions run_bconsole
CALL scripts\functions check_for_zombie_jobs storage=File
CALL scripts\functions stop_bareos

CALL scripts\functions check_two_logs
CALL scripts\functions check_restore_diff
CALL scripts\functions end_test
