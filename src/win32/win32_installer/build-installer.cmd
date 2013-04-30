@ECHO OFF
SETLOCAL

SET CWD=%CD%
CD %1..\..\..
SET TOP_DIR=%CD%
CD %CWD%
SET DEPKG_DIR=%TOP_DIR%\depkgs-msvc
SET DOCS_DIR=%TOP_DIR%\docs\manual

SET BACULA_DLLS=bacula.dll cats_mysql.dll cats_postgresql.dll cats_sqlite3.dll
SET BACULA_EXES=bacula-dir.exe bacula-fd.exe bacula-sd.exe bconsole.exe wx-console.exe
SET BACULA_TOOLS=bcopy.exe bextract.exe bls.exe bscan.exe bsleep.exe bsmtp.exe btape.exe dbcheck.exe scsilist.exe

SET DEP_DLLS=libeay32.dll pthreadVCE.dll ssleay32.dll zlib1.dll zlib1.dll.manifest wxbase28_vc_bacula.dll wxmsw28_core_vc_bacula.dll libmysql.dll libpq.dll comerr32.dll libintl-2.dll libiconv-2.dll krb5_32.dll
SET DEP_EXES=openssl.exe loaderinfo.exe mt.exe mtx.exe scsitape.exe sed.exe tapeinfo.exe sqlite3.exe sqlite3.exe.manifest expr64.exe snooze.exe mkisofs.exe growisofs.exe dvd-ram-control.exe dvd+rw-booktype.exe dvd+rw-format.exe dvd+rw-mediainfo.exe

SET SCRIPTS=mtx-changer.cmd disk-changer.cmd dvd-handler.cmd

SET CATS_MYSQL_CMD=create_mysql_database.cmd drop_mysql_database.cmd make_mysql_tables.cmd drop_mysql_tables.cmd grant_mysql_privileges.cmd
SET CATS_MYSQL_SQL=make_mysql_tables.sql drop_mysql_tables.sql grant_mysql_privileges.sql
SET CATS_PGSQL_CMD=create_postgresql_database.cmd drop_postgresql_database.cmd make_postgresql_tables.cmd drop_postgresql_tables.cmd grant_postgresql_privileges.cmd
SET CATS_PGSQL_SQL=create_postgresql_database.sql make_postgresql_tables.sql drop_postgresql_tables.sql grant_postgresql_privileges.sql
SET CATS_SQLITE_CMD=create_sqlite3_database.cmd drop_sqlite3_database.cmd make_sqlite3_tables.cmd drop_sqlite3_tables.cmd grant_sqlite3_privileges.cmd
SET CATS_SQLITE_SQL=make_sqlite3_tables.sql
SET CATS_CATALOG=make_mysql_catalog_backup.cmd make_postgresql_catalog_backup.cmd make_sqlite3_catalog_backup.cmd delete_catalog_backup.cmd
SET DIRD_FILES=query.sql
SET SSL_FILES=openssl.cnf

SET DOC_PDF_FILES=%DOCS_DIR%\bacula.pdf %DOCS_DIR%\bacula\*.html %DOCS_DIR%\bacula\*.png %DOCS_DIR%\bacula\*.css
SET DOC_HTML_FILES=%DOCS_DIR%\bacula\*.html %DOCS_DIR%\bacula\*.png %DOCS_DIR%\bacula\*.css

FOR %%i in ( %BACULA_DLLS% )     DO XCOPY %1%2\%%i %1installer\%2\ /y
FOR %%i in ( %BACULA_EXES% )     DO XCOPY %1%2\%%i %1installer\%2\ /y
FOR %%i in ( %BACULA_TOOLS% )    DO XCOPY %1%2\%%i %1installer\%2\ /y

FOR %%i in ( %DEP_DLLS% )        DO XCOPY %DEPKG_DIR%\bin\%%i %1installer\%2\ /y
FOR %%i in ( %DEP_EXES% )        DO XCOPY %DEPKG_DIR%\bin\%%i %1installer\%2\ /y

FOR %%i in ( %SCRIPTS% )         DO XCOPY %1scripts\%%i %1installer\%2\ /y
FOR %%i in ( %CATS_MYSQL_CMD% )  DO XCOPY %1cats\%%i %1installer\%2\ /y
FOR %%i in ( %CATS_MYSQL_SQL% )  DO XCOPY %1cats\%%i %1installer\%2\ /y
FOR %%i in ( %CATS_PGSQL_CMD% )  DO XCOPY %1cats\%%i %1installer\%2\ /y
FOR %%i in ( %CATS_PGSQL_SQL% )  DO XCOPY %1cats\%%i %1installer\%2\ /y
FOR %%i in ( %CATS_SQLITE_CMD% ) DO XCOPY %1cats\%%i %1installer\%2\ /y
FOR %%i in ( %CATS_SQLITE_SQL% ) DO XCOPY %1cats\%%i %1installer\%2\ /y

FOR %%i in ( %CATS_CATALOG% )    DO XCOPY %1cats\%%i %1installer\%2\ /y

FOR %%i in ( %DIRD_FILES% )      DO XCOPY %1..\dird\%%i %1installer\%2\ /y
FOR %%i in ( %SSL_FILES% )       DO XCOPY %DEPKG_DIR%\%%i %1installer\%2\ /y

FOR %%i in ( %DOC_PDF_FILES% )   DO XCOPY %%i %1installer\%2\manual\ /y

FOR %%i in ( %DOC_HTML_FILES% )  DO XCOPY %%i %1installer\%2\manual\bacula\ /y

XCOPY %4\x86\Microsoft.VC80.CRT\msvcm80.dll %1installer\%2\ /y
XCOPY %4\x86\Microsoft.VC80.CRT\msvcp80.dll %1installer\%2\ /y
XCOPY %4\x86\Microsoft.VC80.CRT\msvcr80.dll %1installer\%2\ /y
XCOPY %4\x86\Microsoft.VC80.CRT\Microsoft.VC80.CRT.manifest %1installer\%2\ /y
XCOPY %4\Debug_NonRedist\x86\Microsoft.VC80.DebugCRT\msvcm80d.dll %1installer\%2\ /y
XCOPY %4\Debug_NonRedist\x86\Microsoft.VC80.DebugCRT\msvcp80d.dll %1installer\%2\ /y
XCOPY %4\Debug_NonRedist\x86\Microsoft.VC80.DebugCRT\msvcr80d.dll %1installer\%2\ /y
XCOPY %4\Debug_NonRedist\x86\Microsoft.VC80.DebugCRT\Microsoft.VC80.DebugCRT.manifest %1installer\%2\ /y

PATH %DEPKG_DIR%\nsis;%DEPKG_DIR%\tools;%PATH%

FOR /F %%i IN ( 'sed -ne "s/.*[ \t]VERSION[ \t][ \t]*\x22\(.*\)\x22/\1/p" ^< ..\..\version.h' ) DO @SET VERSION=%%i 
makensis /V3 /DVERSION=%VERSION% /DSRC_DIR=%1installer\%2 /DOUT_DIR=%1%2 /DBUILD_TOOLS=%3 winbacula.nsi
EXIT /B %ERRORLEVEL%
