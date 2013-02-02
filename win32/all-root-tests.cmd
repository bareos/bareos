REM
REM Run all root tests
REM
ECHO.
ECHO. >>test.out
ECHO Start root tests
ECHO Start root tests >>test.out
CALL tests\dev-test-root
CALL tests\etc-test-root
CALL tests\lib-test-root
CALL tests\usr-tape-root
ECHO End root tests
ECHO End root tests >>test.out
