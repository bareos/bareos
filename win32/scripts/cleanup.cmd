@ECHO off
REM
REM Cleanup left over files -- both before and after test run
REM

CALL scripts\bareos stop >nul 2>&1

DEL /q /f \tmp\FileVolume* tmp\FileVolume* >nul 2>&1
DEL /q /f \tmp\TestVolume001 \tmp\Small* >nul 2>&1
RD /s /q \tmp\bareos-restores tmp\bareos-restores >nul 2>&1
DEL /q /f tmp\original tmp\Small* tmp\TestVolume* >nul 2>&1
DEL /q /f tmp\restored tmp\largefile tmp\bscan.bsr tmp\log*.out >nul 2>&1
DEL /q /f \tmp\sed_tmp \tmp\file-list >nul 2>&1
RD /s /q tmp\build >nul 2>&1
DEL /q /f tmp\restore-list tmp\restore2-list >nul 2>&1
DEL /q /f tmp\fd.out tmp\dir.out tmp\sd.out >nul 2>&1
DEL /q /f working\log tmp\TEST-* >nul 2>&1
DEL /q /f working\*.bsr >nul 2>&1
DEL /q /f tmp\disk-changer.cmd \tmp\disk-changer.cmd >nul 2>&1

CALL bin\drop_bareos_tables >nul 2>&1
CALL bin\make_bareos_tables >nul 2>&1
CALL bin\grant_bareos_privileges >nul 2>&1
