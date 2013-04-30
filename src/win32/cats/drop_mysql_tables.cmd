@ECHO off
REM
REM Script to delete Bacula tables for MySQL
REM

"@SQL_BINDIR@\mysql" %* < "@bin_dir_cmd@\drop_mysql_tables.sql"
SET RESULT=%ERRORLEVEL%
IF %RESULT% GTR 0 goto :ERROR
ECHO Deletion of Bacula MySQL tables succeeded.
EXIT /b 0

:ERROR
ECHO Deletion of Bacula MySQL tables failed.
EXIT /b %RESULT%
