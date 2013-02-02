REM  /home/kern/bareos/bin/startmysql
CALL make setup
ECHO. >test.out
TYPE build/config.out >>test.out
ECHO. >>test.out
ECHO Test results >>test.out
ECHO. >>test.out
python starttime.py
CALL all-non-root-tests
ECHO.
ECHO End do_file tests
ECHO End do_file tests >>test.out
CALL scripts/cleanup
TYPE test.out
python endtime.py
