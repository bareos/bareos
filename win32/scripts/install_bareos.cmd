REM Arguments:
REM    1 = Debug or Release
REM    2 = email address
REM    3 = mail server
REM    4 = tape device
REM    5 = changer device or nul
REM    6 = tape1 device or nul

SETLOCAL ENABLEDELAYEDEXPANSION

SET CATS=create_sqlite3_database.cmd drop_sqlite3_database.cmd drop_sqlite3_tables.cmd grant_sqlite3_privileges.cmd make_sqlite3_catalog_backup.cmd make_sqlite3_tables.cmd

SET CONFS=bareos-dir.conf.in bareos-fd.conf.in bareos-sd.conf.in bconsole.conf.in

CALL scripts\bareos stop

IF NOT EXIST bin MKDIR bin

COPY build\src\win32\installer\%1\*.exe bin >nul
COPY build\src\win32\installer\%1\*.dll bin >nul
COPY build\src\win32\installer\%1\*.manifest bin >nul
COPY build\src\win32\%1\*.pdb bin >nul
COPY build\src\win32\installer\%1\query.sql bin >nul

COPY bin\cats_sqlite3.dll bin\bareos_cats.dll >nul

FOR /f %%i IN ( 'cmd /c openssl.exe rand -base64 33 2^>nul' ) DO SET CLIENT_PASSWORD=%%i
FOR /f %%i IN ( 'cmd /c openssl.exe rand -base64 33 2^>nul' ) DO SET STORAGE_PASSWORD=%%i
FOR /f %%i IN ( 'cmd /c openssl.exe rand -base64 33 2^>nul' ) DO SET DIRECTOR_PASSWORD=%%i
FOR /f %%i IN ( 'cmd /c openssl.exe rand -base64 33 2^>nul' ) DO SET MONITOR_PASSWORD=%%i

ECHO s;@director_name@;localhost-dir;g >install.sed
ECHO s;@director_port@;8101;g >>install.sed
ECHO s;@storage_name@;localhost-sd;g >>install.sed
ECHO s;@storage_port@;8103;g >>install.sed
ECHO s;@client_name@;localhost-fd;g >>install.sed
ECHO s;@client_port@;8102;g >>install.sed

REM ECHO s;@DISTVER@;$R2;

ECHO s;@working_dir@;%CD:\=\\\\%\\\\working;g >>install.sed
ECHO s;@working_dir_cmd@;%CD:\=\\%\\working;g >>install.sed
ECHO s;@bin_dir@;%CD:\=\\\\%\\\\bin;g >>install.sed;
ECHO s;@bin_dir_cmd@;%CD:\=\\%\\bin;g >>install.sed
ECHO s;@BUILD_DIR@;%CD:\=/%/build;g >>install.sed
ECHO s;@director_address@;localhost;g >>install.sed
ECHO s;@client_address@;localhost;g >>install.sed
ECHO s;@storage_address@;localhost;g >>install.sed
ECHO s;@client_maxjobs@;2;g >>install.sed
ECHO s;@client_password@;%CLIENT_PASSWORD%;g >>install.sed
ECHO s;@storage_maxjobs@;10;g >>install.sed
ECHO s;@storage_password@;%STORAGE_PASSWORD%;g >>install.sed
ECHO s;@director_maxjobs@;1;g >>install.sed
ECHO s;@director_password@;%DIRECTOR_PASSWORD%;g >>install.sed
ECHO s;@job_email@;%2;g >>install.sed
ECHO s;@smtp_host@;%3;g >>install.sed
ECHO s;@monitor_name@;localhost-mon;g >>install.sed
ECHO s;@monitor_password@;%MONITOR_PASSWORD%;g >>install.sed
ECHO s;@tape_drive@;%4;g >>install.sed
ECHO s;@autochanger@;%5;g >>install.sed
ECHO s;@changer_path@;%6;g >>install.sed
ECHO s;@tape_drive1@;%7;g >>install.sed

FOR %%i in ( %CATS% ) DO (
   SET NAME=%%i
   SET TARGET=bin\!NAME:sqlite3=bareos!
   sed -f install.sed build\src\win32\installer\%1\!NAME! > !TARGET!
)

FOR %%i in ( %CONFS% ) DO sed -f install.sed build\src\win32\installer\%%i > bin\%%~ni

sed -f install.sed build\src\win32\installer\%1\make_sqlite3_tables.sql > bin\make_sqlite3_tables.sql
sed -f install.sed build\src\win32\installer\%1\mtx-changer.cmd > bin\mtx-changer.cmd

CALL scripts\bareos uninstall
CALL scripts\bareos install %CD%\bin
