REM
REM Run a backup of the full bareos build directory, but with the
REM   Maximum File Size set. Then do a restore of a few files to kick in
REM   disk seeking (not yet enabled), and ensure that the restored files
REM   match. Even though disk seeking is not yet enabled, this is a good test,
REM   and once it is enabled, this will test it.
REM
SET TestName=restore-disk-seek-test
SET JobName=restore-disk-seek

CALL scripts\functions set_debug 1
CALL scripts\functions copy_test_confs

ECHO %CD:\=/%/tmp/build >tmp\file-list

IF EXIST tmp\build RD /s /q  tmp\build
MKDIR tmp\build

REM Copy only the .c files (to be restored)
REM  set files to "*.c" for all c files
SET files=ua_tree.c ua_update.c
REM SET files=*.c
FOR %%i in ( %files% ) DO COPY build\src\dird\%%i tmp\build >nul 2>&1
DIR /b tmp\build >tmp\1
sed -e "s;^;%CD:\=/%/tmp/build/;" tmp\1 >tmp\restore-list

REM At this point restore-list contains the list
REM  of files we will restore
DEL tmp\1

REM Now backup *everything*
IF EXIST tmp\build RD /s /q  tmp\build
MKDIR tmp\build
COPY build\src\dird\* tmp\build >nul 2>&1
REM Enable MaximumFileSize
COPY bin\bareos-sd.conf tmp\1 >nul 2>&1
sed "s;# Maximum File Size;  Maximum File Size;" tmp\1 >bin\bareos-sd.conf

CALL scripts\functions change_jobname CompressedTest %JobName%
CALL scripts\functions start_test

sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\restore-disk-seek-test.bscr >tmp\bconcmds

CALL scripts\functions run_bareos
CALL scripts\functions check_for_zombie_jobs storage=File
CALL scripts\functions stop_bareos

REM Now setup a control directory of only what we *should* restore
RD /s /q tmp\build
MKDIR tmp\build
FOR %%i in ( %files% ) DO COPY build\src\dird\%%i tmp\build >nul 2>&1

CALL scripts\functions check_two_logs
CALL scripts\functions check_restore_tmp_build_diff
CALL scripts\functions end_test
