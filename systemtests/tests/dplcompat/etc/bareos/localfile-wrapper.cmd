@echo off
rem divert call to bash-script

bash %~dp0/localfile-wrapper.sh %*
