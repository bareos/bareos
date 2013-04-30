@ECHO off
REM
REM Script to create Bacula SQLite tables

ECHO .databases | "@bin_dir_cmd@\sqlite3" %* "@working_dir_cmd@\bacula.db"
EXIT /b 0
