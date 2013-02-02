python starttime.py
IF NOT EXIST bin\tape_options touch bin/tape_options
ECHO. >test.out
TYPE build/config.out >>test.out
ECHO. >>test.out
ECHO Test results >>test.out
ECHO. >>test.out
python starttime.py
CALL all-non-root-tests
CALL all-non-root-tape-tests
CALL all-non-root-2tape-tests
ECHO.
ECHO End do_all tests
ECHO End do_all tests >>test.out
CALL scripts/cleanup
TYPE test.out
python endtime.py
