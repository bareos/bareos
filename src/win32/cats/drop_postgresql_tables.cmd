@ECHO off
REM
REM  Script to delete Bacula tables for PostgreSQL
REM

"@SQL_BINDIR@\psql" -f "@bin_dir_cmd@\drop_postgresql_tables.sql" -d bacula %*
IF ERRORLEVEL 1 GOTO :ERROR
ECHO Deletion of Bacula PostgreSQL tables succeeded.
EXIT /b 0
GOTO :EOF

:ERROR
ECHO Deletion of Bacula PostgreSQL tables failed.
EXIT /b 1
