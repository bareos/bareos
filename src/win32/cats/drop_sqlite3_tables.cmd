@ECHO off
REM
REM Script to delete the SQLite Bacula database (same as deleting 
REM the tables)
REM

DEL "@working_dir_cmd@\bacula.db"
EXIT /b 0
