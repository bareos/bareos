@ECHO off
REM
REM Script to grant privileges to the bacula database
REM

"@SQL_BINDIR@\mysql" -u root -f %* < "@bin_dir_cmd@\grant_mysql_privileges.sql"
SET RESULT=%ERRORLEVEL%
IF %RESULT% GTR 0 GOTO :ERROR
ECHO Privileges for bacula granted.
EXIT /b 0

:ERROR
ECHO Error creating privileges.
EXIT /b %RESULT%
