@ECHO off
REM
REM  Script to create Bacula PostgreSQL tables
REM

"@SQL_BINDIR@\psql" -f "@bin_dir_cmd@\make_postgresql_tables.sql" -d bacula %*
IF ERRORLEVEL 1 GOTO :ERROR
ECHO Creation of Bacula PostgreSQL tables succeeded.
EXIT /b 0
GOTO :EOF

:ERROR
ECHO Creation of Bacula PostgreSQL tables failed.
EXIT /b 1
