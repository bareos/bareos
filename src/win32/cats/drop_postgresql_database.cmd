@ECHO off
REM
REM  Script to drop Bacula database(s)
REM

"@SQL_BINDIR@\dropdb" %* bacula
IF ERRORLEVEL 1 GOTO :ERROR
ECHO Drop of bacula database succeeded.
EXIT /b 0
GOTO :EOF

:ERROR
ECHO Drop of bacula database failed.
EXIT /b 1
