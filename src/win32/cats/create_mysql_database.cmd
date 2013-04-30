@ECHO off
REM
REM Script to create Bacula database(s)
REM

"@SQL_BINDIR@\mysql" %* -e "CREATE DATABASE bacula;"
SET RESULT=%ERRORLEVEL%
IF %RESULT% GTR 0 GOTO :ERROR
ECHO Creation of bacula database succeeded.
EXIT /b 0

:ERROR
ECHO Creation of bacula database failed.
EXIT /b %RESULT%
