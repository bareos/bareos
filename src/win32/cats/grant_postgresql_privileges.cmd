@ECHO off
REM
REM  Script to grant privileges to the bacula database
REM
USER=bacula

"@SQL_BINDIR@\psql" -f "@bin_dir_cmd@\grant_postgresql_privileges.sql" -d bacula %*
IF ERRORLEVEL 1 GOTO :ERROR
ECHO Error creating privileges.
EXIT /b 0
GOTO :EOF

:ERROR
ECHO Drop of bacula database failed.
EXIT /b 1
