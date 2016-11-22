@echo off

SET CMD=%0
SET SRC=%~1
SET DST=%~2

SET DIR=%~dp0
SET SED_CMD=%DIR%\sed.exe
SET SED_SCRIPT=%ALLUSERSPROFILE%\Bareos\configure.sed

if not exist "%SED_CMD%" (
    echo Failed to find the command sed. It is expected in the same directory as %CMD% [%SED%].
    call :exit 1
)

echo source: %SRC%
echo dest:   %DST%

if "%SRC%" == "" (
    call :usage
)

if not exist "%SRC%\*" (
    echo Directory "%SRC%" does not exists.
    call :exit 1
)

if not exist "%SED_SCRIPT%" (
    echo Configure configuration [%SED_SCRIPT%] does not exists.
    call :exit 1
)

if "%DST%" == "" (
    call :usage
)


rem prepare destination directory
mkdir "%DST%"

if not exist "%DST%\*" (
    echo Directory "%DST%" does not exists.
    call :exit 1
)

rem Check if the icacls command is available.
rem If yes, it will later be used to adapt
rem rem the permissions of the configuration.
WHERE icacls.exe >NUL
IF ERRORLEVEL 0 (
   SET ACL_CMD=icacls.exe
)

pushd "%SRC%"
rem c: COMPONENT
for /D %%c in (*) do (
    popd
    echo component: %%c
    if exist "%DST%\%%c" (
        echo.  skipped, path %DST%\%%c already exists
    ) else (
        rem copy config templates to destination
        rem xcopy options:
        rem /E           Copies directories and subdirectories, including empty ones.
        rem /I           If destination does not exist and copying more than one file,
        rem                assumes that destination must be a directory.
        rem /O           Copies file ownership and ACL information.
        rem /Y           Suppresses prompting to confirm you want to overwrite
        rem                and existing destination file.
        xcopy /e /i /o /y "%SRC%\%%c" "%DST%\%%c"
        rem r: RESOURCE TYPE
        for /D %%r in ("%DST%\%%c"\*) do (
            echo.  resource: %%r
            rem f: file
            for %%f in ("%%r"\*.conf) do (
                echo.    %%f
                rem delete old backup
                if exist "%%f.orig" del "%%f.orig"
                "%SED_CMD%" --in-place=".orig" --file "%SED_SCRIPT%" "%%f"
            )
        )
        if defined ACL_CMD (
            if "%%c" == "tray-monitor.d" (
               rem The bareos-tray-monitor should be accessable and its configuration adaptable
               rem for all users.
               rem Therefore grant Full Access to the group "Users".
               "%ACL_CMD%" "%DST%\%%c" /grant *S-1-5-32-545:"(OI)(CI)F"
            ) else (
               rem The Bareos Daemon (Dir, Sd, Fd) contains passwords.
               rem Only users from the "Administrators" group should be able to read them.
               rem Therefore remove the "Users" group from the ACLs.

               rem Stop ACL inheritance and copy the current ACLs
               "%ACL_CMD%" "%DST%\%%c" /inheritance:d

               rem Remove ACL for group "Users"
               "%ACL_CMD%" "%DST%\%%c" /remove:g *S-1-5-32-545 /T
            )
        )
    )
    pushd "%SRC%"
)
popd

if defined ACL_CMD (
   rem configure.sed also contains passwords.
   rem So make it only accessable for Administrators.

   rem Stop ACL inheritance and copy the current ACLs
   "%ACL_CMD%" "%DST%\configure.sed" /inheritance:d

   rem Remove ACL for group "Users"
   "%ACL_CMD%" "%DST%\configure.sed" /remove:g *S-1-5-32-545 /T
)

rem call :exit
goto:eof

rem functions

:usage
    echo %CMD% source dest
    call :exit 1

:exit
    rem pause
    rem exit %*
    rem emulate Ctrl-C
    cmd /c exit -1073741510
