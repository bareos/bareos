REM
REM Run all tape tests
REM
CALL config_var
IF NOT "%AUTOCHANGER%" == "nul" mtx -f %AUTOCHANGER% load 1 >nul 2>&1
ECHO.
ECHO. >>test.out
ECHO Start all non-root tape tests
ECHO Start all non-root tape tests >>test.out
REM CALL tests\ansi-label-tape
CALL tests\backup-bareos-tape
CALL tests\btape-fill-tape
CALL tests\fixed-block-size-tape
CALL tests\four-concurrent-jobs-tape
CALL tests\four-jobs-tape
CALL tests\incremental-tape
CALL tests\relabel-tape
CALL tests\restore-by-file-tape
CALL tests\small-file-size-tape
CALL tests\truncate-bug-tape
CALL tests\two-pool-tape
CALL tests\2drive-incremental-2tape
CALL tests\bscan-tape
CALL tests\verify-vol-tape
ECHO End all non-root tape tests
ECHO End all non-root tape tests >>test.out
