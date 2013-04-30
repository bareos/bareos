@ECHO off
REM
REM Script to create Bacula SQLite tables

"@bin_dir_cmd@\sqlite3" %* "@working_dir_cmd@\bacula.db" < "@bin_dir_cmd@\make_sqlite3_tables.sql"

EXIT /b 0
