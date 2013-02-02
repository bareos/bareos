REM
REM Run all tape tests
REM
ECHO Start two tape tests
ECHO Start two tape tests >>test.out
CALL tests\two-volume-tape
CALL tests\incremental-2tape
ECHO End two tape tests
ECHO End two tape tests >>test.out
