@ECHO off
REM 
REM  This script dumps your Bacula catalog in ASCII format
REM  It works for MySQL, SQLite, and PostgreSQL
REM 
REM   %1 is the name of the database to be backed up and the name
REM      of the output file (default = bacula
REM   %2 is the user name with which to access the database
REM      (default = bacula).
REM   %3 is the password with which to access the database or "" if no password
REM      (default "")
REM 
REM 
@ECHO on

DEL /f "@working_dir_cmd@\%1.sql" 2>nul

ECHO .dump | "@bin_dir_cmd@\sqlite3" "@working_dir_cmd@\%1.db" > "@working_dir_cmd@\%1.sql"

@ECHO off
REM 
REM   To read back a MySQL database use: 
REM      cd @working_dir_cmd@
REM      rd /s /q @SQL_BINDIR@\..\data\bacula
REM      mysql < bacula.sql
REM 
REM   To read back a SQLite database use:
REM      cd @working_dir_cmd@
REM      del /f bacula.db
REM      sqlite bacula.db < bacula.sql
REM 
REM   To read back a PostgreSQL database use:
REM      cd @working_dir_cmd@
REM      dropdb bacula
REM      createdb -T template0 -E SQL_ASCII bacula
REM      psql bacula < bacula.sql
REM 
EXIT /b 0
