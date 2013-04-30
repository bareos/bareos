@ECHO off
REM
REM Script to create Bacula MySQL tables
REM

"@SQL_BINDIR@\mysql" -f %* < "@bin_dir_cmd@\make_mysql_tables.sql"
SET RESULT=%ERRORLEVEL%
IF %RESULT% GTR 0 GOTO :ERROR
ECHO Creation of Bacula MySQL tables succeeded.
EXIT /b 0

:ERROR
ECHO Creation of Bacula MySQL tables failed.
EXIT /b %RESULT%
