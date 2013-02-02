@ECHO off
REM
REM First argument is expected to be a user's configuration file
REM
IF "%1" == "" (
   ECHO Arg1 must specify a config file ^(e.g. prototype.conf^)
   EXIT /b 1
)

COPY %1 config_var.cmd
