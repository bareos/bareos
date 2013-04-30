@ECHO off
REM
REM  Script to create Bacula database(s)
REM

REM use SQL_ASCII to be able to put any filename into
REM  the database even those created with unusual character sets
SET ENCODING=ENCODING 'SQL_ASCII'

REM use UTF8 if you are using standard Unix/Linux LANG specifications
REM  that use UTF8 -- this is normally the default and *should* be
REM  your standard.  Bacula consoles work correctly *only* with UTF8.
REM SET ENCODING=ENCODING 'UTF8'

"@SQL_BINDIR@\psql" -f "@bin_dir_cmd@\create_postgresql_database.sql" -d template1 %*
IF ERRORLEVEL 1 GOTO :ERROR
ECHO Creation of bacula database succeeded.
EXIT /b 0
GOTO :EOF

:ERROR
ECHO Creation of bacula database failed.
EXIT /b 1
