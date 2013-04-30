@ECHO off
REM
REM Script to drop Bacula database(s)
REM

"@SQL_BINDIR@\mysql" %* -f -e "DROP DATABASE bacula;"
SET RESULT=%ERRORLEVEL%
IF %RESULT% GTR 0 GOTO :ERROR
ECHO Drop of bacula database succeeded.
EXIT /b 0

:ERROR
ECHO Drop of bacula database failed.
EXIT /b %RESULT%
