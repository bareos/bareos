IF "%1" == "" GOTO :usage
IF "%2" == "" GOTO :usage
IF "%3" == "" GOTO :usage
IF "%4" == "" GOTO :usage
IF "%5" == "" GOTO :usage
IF "%6" == "" GOTO :usage
GOTO :args_ok

:usage
ECHO First arg must be email name
ECHO   and the second must be the smtp or email host
ECHO   and the third must be a tape drive
ECHO   and the fourth must be a tape control name or nul
ECHO   and the fifth must be the full path to the mtx program
ECHO   and the sixth must be tape drive 1 or nul
EXIT /b 1

:args_ok
SET out=tmp\sed_tmp

REM Create sed command script

ECHO s;@sbindir@;%CD:\=/%/bin;g >%out%
ECHO s;@scriptdir@;%CD:\=/%/scripts;g >>%out%
ECHO s;@working_dir@;%CD:\=/%/working;g >>%out%
ECHO s;@piddir@;%CD:\=/%/working;g >>%out%
ECHO s;@job_email@;%1;g >>%out%
ECHO s;@smtp_host@;%2;g >>%out%
ECHO s;@tape_drive@;%3;g >>%out%
ECHO s;@autochanger@;%4;g >>%out%
ECHO s;@tmpdir@;%CD:\=/%/tmp;g >>%out%
ECHO s;@hostname@;localhost;g >>%out%
ECHO s;@changer_path@;%5;g >>%out%
ECHO s;@tape_drive1@;%6;g >>%out%
ECHO s;@disk_drive@;%CD:\=/%/tmp/disk-changer;g >>%out%
ECHO s;@smtp_program@;%CD:\=/%/bin/bsmtp;g >>%out%

ECHO SET AUTOCHANGER=%4 >config_out.cmd
ECHO SET AUTOCHANGER_PATH=%5 >>config_out.cmd
ECHO SET TAPE_DRIVE1=%6 >>config_out.cmd

REM process .in files with sed script
sed -f %out% scripts\test-bareos-dir.conf.in >scripts\test-bareos-dir.conf
sed -f %out% scripts\new-test-bareos-dir.conf.in >scripts\new-test-bareos-dir.conf
sed -f %out% scripts\testa-bareos-dir.conf.in >scripts\testa-bareos-dir.conf
sed -f %out% scripts\test-bareos-fd.conf.in >scripts\test-bareos-fd.conf
sed -f %out% scripts\test-bareos-sd.conf.in >scripts\test-bareos-sd.conf
sed -f %out% scripts\test-console.conf.in >scripts\test-console.conf
sed -f %out% scripts\crypto-bareos-fd.conf.in >scripts\crypto-bareos-fd.conf
sed -f %out% scripts\bareos-dir-tape.conf.in >scripts\bareos-dir-tape.conf
sed -f %out% scripts\bareos-dir-migration.conf.in >scripts\bareos-dir-migration.conf
sed -f %out% scripts\win32-bareos-dir-tape.conf.in >scripts\win32-bareos-dir-tape.conf
sed -f %out% scripts\bareos-sd-tape.conf.in >scripts\bareos-sd-tape.conf
sed -f %out% scripts\bareos-sd-2tape.conf.in >scripts\bareos-sd-2tape.conf
sed -f %out% scripts\bareos-sd-migration.conf.in >scripts\bareos-sd-migration.conf
sed -f %out% scripts\bareos-sd-2disk.conf.in >scripts\bareos-sd-2disk.conf
sed -f %out% scripts\bareos-sd-2drive.conf.in >scripts\bareos-sd-2drive.conf
sed -f %out% scripts\bareos-sd-2disk-drive.conf.in >scripts\bareos-sd-2disk-drive.conf
sed -f %out% scripts\cleanup-tape.cmd.in >scripts\cleanup-tape.cmd
sed -f %out% scripts\cleanup-2tape.cmd.in >scripts\cleanup-2tape.cmd
sed -f %out% scripts\cleanup-2drive.cmd.in >scripts\cleanup-2drive.cmd
sed -f %out% scripts\prepare-two-tapes.cmd.in >scripts\prepare-two-tapes.cmd
sed -f %out% scripts\bareos-dir.conf.testrunscript.in >scripts\bareos-dir.conf.testrunscript

COPY bin\bareos-sd.conf tmp\bareos-sd.tmp >nul
sed -e "s;C:\\Temp;%CD:\=/%/tmp;g" tmp\bareos-sd.tmp >bin\bareos-sd.conf

REM get proper SD tape definitions
COPY scripts\win32_tape_options bin\tape_options >nul

DEL %out%
DEL tmp\*.tmp
