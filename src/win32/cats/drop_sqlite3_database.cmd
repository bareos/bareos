@ECHO off
REM
REM Script to drop Bacula SQLite tables

DEL "@working_dir_cmd@\bacula.db"
ECHO SQLite database dropped.
EXIT /b 0
