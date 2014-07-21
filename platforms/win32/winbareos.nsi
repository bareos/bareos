;
;   BAREOS?? - Backup Archiving REcovery Open Sourced
;
;   Copyright (C) 2012-2013 Bareos GmbH & Co. KG
;
;   This program is Free Software; you can redistribute it and/or
;   modify it under the terms of version three of the GNU Affero General Public
;   License as published by the Free Software Foundation and included
;   in the file LICENSE.
;
;   This program is distributed in the hope that it will be useful, but
;   WITHOUT ANY WARRANTY; without even the implied warranty of
;   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
;   Affero General Public License for more details.
;
;   You should have received a copy of the GNU Affero General Public License
;   along with this program; if not, write to the Free Software
;   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
;   02110-1301, USA.

RequestExecutionLevel admin

!addplugindir ../nsisplugins

; HM NIS Edit Wizard helper defines
!define PRODUCT_NAME "Bareos"
#!define PRODUCT_VERSION "1.0"
!define PRODUCT_PUBLISHER "Bareos GmbH & Co.KG"
!define PRODUCT_WEB_SITE "http://www.bareos.com"
!define PRODUCT_DIR_REGKEY "Software\Microsoft\Windows\CurrentVersion\App Paths\bareos-fd.exe"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"

SetCompressor lzma

# variable definitions
Var LocalHostAddress
Var HostName

# Config Parameters Dialog

# Needed for Configuring client config file
Var ClientName            #XXX_REPLACE_WITH_HOSTNAME_XXX
Var ClientPassword        #XXX_REPLACE_WITH_FD_PASSWORD_XXX
Var ClientMonitorPassword #XXX_REPLACE_WITH_FD_MONITOR_PASSWORD_XXX
Var ClientAddress         #XXX_REPLACE_WITH_FD_MONITOR_PASSWORD_XXX
Var ClientCompatible      # is  client compatible?

# Needed for configuring the storage config file
Var StorageName        # name of the storage in the director config (Device)
Var StorageDaemonName  # name of the storage daemon
Var StoragePassword
Var StorageMonitorPassword
Var StorageAddress


# Needed for bconsole and bat:
Var DirectorAddress       #XXX_REPLACE_WITH_HOSTNAME_XXX
Var DirectorPassword      #XXX_REPLACE_WITH_DIRECTOR_PASSWORD_XXX
Var DirectorName
Var DirectorMonPassword

# Needed for Postgresql Database
Var PostgresPath
Var PostgresBinPath
Var DbDriver
Var DbPassword
Var DbPort
Var DbUser
Var DbName
Var DbEncoding

# Generated configuration snippet for bareos director config (client ressource)
Var ConfigSnippet

Var OsIsNT
Var dialog
Var hwnd

# Keep the configuration files also when running silently?
Var SilentKeepConfig

# Are we doing an upgrade or fresh install?
# If we are doing an upgrade we automatically
# keep the existing config files silently and skip
# the param dialogs and the config snippet dialog
Var Upgrading

!include "LogicLib.nsh"
!include "FileFunc.nsh"
!include "Sections.nsh"
!include "StrFunc.nsh"
!include "WinMessages.nsh"
!include "nsDialogs.nsh"
!include "x64.nsh"
!include "WinVer.nsh"
!include "WordFunc.nsh"

# call functions once to have them included
${StrCase}
${StrTrimNewLines}
${StrRep}

; MUI 1.67 compatible ------
!include "MUI.nsh"

; MUI Settings
!define MUI_ABORTWARNING
!define MUI_ICON   "bareos.ico"
!define MUI_UNICON "bareos.ico"
#!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
#!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"
!define MUI_COMPONENTSPAGE_SMALLDESC


!insertmacro GetParameters
!insertmacro GetOptions

; Welcome page
!insertmacro MUI_PAGE_WELCOME

; License
!insertmacro MUI_PAGE_LICENSE "LICENSE"

; Directory page
!insertmacro MUI_PAGE_DIRECTORY

; Components page
!insertmacro MUI_PAGE_COMPONENTS

; Check if database server is installed
Page custom checkForDatabase

; Custom für Abfragen benötigter Parameter für den Client
Page custom getClientParameters

; Custom für Abfragen benötigter Parameter für den Zugriff auf director
Page custom getDirectorParameters

; Custom für Abfragen benötigter Parameter für den Storage
Page custom getStorageParameters

; Instfiles page
!insertmacro MUI_PAGE_INSTFILES
; Finish page

; Custom page shows director config snippet
Page custom displayDirconfSnippet

Function LaunchLink
  ExecShell "open" "http://www.bareos.com"
FunctionEnd

!define MUI_FINISHPAGE_RUN
#!define MUI_FINISHPAGE_RUN_NOTCHECKED
!define MUI_FINISHPAGE_RUN_TEXT "Open www.bareos.com"
!define MUI_FINISHPAGE_RUN_FUNCTION "LaunchLink"

!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_INSTFILES

; Language files
!insertmacro MUI_LANGUAGE "English"

; Reserve files
!insertmacro MUI_RESERVEFILE_INSTALLOPTIONS

; MUI end ------


#
# move existing conf files to .old
# and install new ones in original place
# also, install a shortcut to edit
# the conf file
#
!macro InstallConfFile fname
  # This is important to have $APPDATA variable
  # point to ProgramData folder
  # instead of current user's Roaming folder
  SetShellVarContext all

  ${If} ${FileExists} "$APPDATA\${PRODUCT_NAME}\${fname}"
    StrCmp $SilentKeepConfig "yes" keep # in silent install keep configs silently if desired
    StrCmp $Upgrading "yes" keep        # during upgrade also keep configs silently

    MessageBox MB_YESNO|MB_ICONQUESTION \
      "Existing config file found: $APPDATA\${PRODUCT_NAME}\${fname}$\r$\nKeep existing config file?" \
      /SD IDNO IDYES keep IDNO move
move:
    Rename "$APPDATA\${PRODUCT_NAME}\${fname}" "$APPDATA\${PRODUCT_NAME}\${fname}.old"
    Rename "$PLUGINSDIR\${fname}" "$APPDATA\${PRODUCT_NAME}\${fname}"
    MessageBox MB_OK|MB_ICONINFORMATION \
      "Existing config file saved as $APPDATA\${PRODUCT_NAME}\${fname}.old" \
      /SD IDOK
    GOTO +3
keep:
    Rename "$PLUGINSDIR\${fname}" "$APPDATA\${PRODUCT_NAME}\${fname}.new"
    StrCmp $Upgrading "yes" skipmsgbox        # during upgrade keep existing file without messagebox
    MessageBox MB_OK|MB_ICONINFORMATION \
      "New config file stored $APPDATA\${PRODUCT_NAME}\${fname}.new" \
      /SD IDOK
skipmsgbox:
  ${Else}
    Rename "$PLUGINSDIR\${fname}" "$APPDATA\${PRODUCT_NAME}\${fname}"
  ${EndIf}
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Edit ${fname}.lnk" "write.exe" '"$APPDATA\${PRODUCT_NAME}\${fname}"'

  # for traymonitor.conf, set access and ownership to users
  ${If} ${fname} == "tray-monitor.conf"
    # Disable file access inheritance
    AccessControl::DisableFileInheritance "$APPDATA\${PRODUCT_NAME}\${fname}"
    Pop $R0
    ${If} $R0 == error
       Pop $R0
       DetailPrint `AccessControl error: $R0`
    ${EndIf}

    # Set file owner to administrator
    AccessControl::SetFileOwner "$APPDATA\${PRODUCT_NAME}\${fname}" "(S-1-5-32-545)"  # user
    Pop $R0
    ${If} $R0 == error
       Pop $R0
       DetailPrint `AccessControl error: $R0`
    ${EndIf}

    # Set fullaccess only for administrators (S-1-5-32-544)
    AccessControl::ClearOnFile "$APPDATA\${PRODUCT_NAME}\${fname}" "(S-1-5-32-545)" "FullAccess"
    Pop $R0
    ${If} $R0 == error
       Pop $R0
       DetailPrint `AccessControl error: $R0`
    ${EndIf}
  ${Else}
    # All other config files admin owner and only access
    # Disable file access inheritance
    AccessControl::DisableFileInheritance "$APPDATA\${PRODUCT_NAME}\${fname}"
    Pop $R0
    ${If} $R0 == error
       Pop $R0
       DetailPrint `AccessControl error: $R0`
    ${EndIf}

    # Set file owner to administrator
    AccessControl::SetFileOwner "$APPDATA\${PRODUCT_NAME}\${fname}" "(S-1-5-32-544)"  # administratoren
    Pop $R0
    ${If} $R0 == error
       Pop $R0
       DetailPrint `AccessControl error: $R0`
    ${EndIf}

    # Set fullaccess only for administrators (S-1-5-32-544)
    AccessControl::ClearOnFile "$APPDATA\${PRODUCT_NAME}\${fname}" "(S-1-5-32-544)" "FullAccess"
    Pop $R0
    ${If} $R0 == error
       Pop $R0
       DetailPrint `AccessControl error: $R0`
    ${EndIf}
  ${EndIf}
!macroend

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "${PRODUCT_NAME}-${PRODUCT_VERSION}.exe"
InstallDir "$PROGRAMFILES\${PRODUCT_NAME}"
InstallDirRegKey HKLM "${PRODUCT_DIR_REGKEY}" ""
ShowInstDetails show
ShowUnInstDetails show

InstType "Standard"
InstType "Full"
InstType "Minimal"


Section -StopDaemon
#  nsExec::ExecToLog "net stop bareos-fd"
# looks like this does not work on win7 sp1
# if the service doesnt exist, it fails and the installation
# cannot start
# so we use the shotgun:
  KillProcWMI::KillProc "bareos-fd.exe"
  KillProcWMI::KillProc "bareos-sd.exe"
  KillProcWMI::KillProc "bareos-dir.exe"
  KillProcWMI::KillProc "bareos-tray-monitor.exe"
  KillProcWMI::KillProc "bconsole.exe"
  KillProcWMI::KillProc "bat.exe"
SectionEnd


Section -SetPasswords
  SetShellVarContext all
  # Write sed file to replace the preconfigured variables by the configured values
  #
  # File Daemon and Tray Monitor configs
  #
  FileOpen $R1 $PLUGINSDIR\config.sed w
  #FileOpen $R1 config.sed w

  FileWrite $R1 "s#@VERSION@#${PRODUCT_VERSION}#g$\r$\n"
  FileWrite $R1 "s#@DATE@#${__DATE__}#g$\r$\n"
  FileWrite $R1 "s#@DISTNAME@#Windows#g$\r$\n"

  FileWrite $R1 "s#XXX_REPLACE_WITH_DIRECTOR_PASSWORD_XXX#$DirectorPassword#g$\r$\n"
  FileWrite $R1 "s#XXX_REPLACE_WITH_DIRECTOR_MONITOR_PASSWORD_XXX#$DirectorMonPassword#g$\r$\n"
  FileWrite $R1 "s#XXX_REPLACE_WITH_CLIENT_PASSWORD_XXX#$ClientPassword#g$\r$\n"
  FileWrite $R1 "s#XXX_REPLACE_WITH_CLIENT_MONITOR_PASSWORD_XXX#$ClientMonitorPassword#g$\r$\n"

  FileWrite $R1 "s#XXX_REPLACE_WITH_HOSTNAME_XXX#$HostName#g$\r$\n"

  FileWrite $R1 "s#XXX_REPLACE_WITH_BASENAME_XXX-fd#$ClientName#g$\r$\n"
  FileWrite $R1 "s#XXX_REPLACE_WITH_BASENAME_XXX-dir#$DirectorName#g$\r$\n"
  FileWrite $R1 "s#XXX_REPLACE_WITH_BASENAME_XXX-mon#$HostName-mon#g$\r$\n"

  #
  # If we do not want to be compatible we uncomment the setting for compatible
  #
  ${If} $ClientCompatible != ${BST_CHECKED}
    FileWrite $R1 "s@# compatible@compatible@g$\r$\n"
  ${EndIf}

  FileWrite $R1 "s#XXX_REPLACE_WITH_SD_MONITOR_PASSWORD_XXX#$StorageMonitorPassword#g$\r$\n"
  FileWrite $R1 "s#XXX_REPLACE_WITH_STORAGE_PASSWORD_XXX#$StoragePassword#g$\r$\n"
  FileWrite $R1 "s#XXX_REPLACE_WITH_BASENAME_XXX-sd#$StorageDaemonName#g$\r$\n"

  # Director DB Connection Setup

  FileWrite $R1 "s#XXX_REPLACE_WITH_DATABASE_DRIVER_XXX#$DbDriver#g$\r$\n"
  FileWrite $R1 "s#XXX_REPLACE_WITH_DB_PORT_XXX#$DbPort#g$\r$\n"
  FileWrite $R1 "s#XXX_REPLACE_WITH_DB_USER_XXX#$DbUser#g$\r$\n"
  FileWrite $R1 "s#XXX_REPLACE_WITH_DB_PASSWORD_XXX#$DbPassword#g$\r$\n"

  # backupcatalog backup scripts
  ${StrRep} '$0' "$APPDATA" '\' '/' # replace \ with / in APPDATA
  FileWrite $R1 "s#C:/Program Files/Bareos/make_catalog_backup.pl MyCatalog#$0/${PRODUCT_NAME}/scripts/make_catalog_backup.bat#g$\r$\n"
  FileWrite $R1 "s#C:/Program Files/Bareos/delete_catalog_backup#$0/${PRODUCT_NAME}/scripts/delete_catalog_backup.bat#g$\r$\n"


  FileClose $R1

  nsExec::ExecToLog '$PLUGINSDIR\sed.exe -f "$PLUGINSDIR\config.sed" -i-template "$PLUGINSDIR\bareos-dir.conf"'
  nsExec::ExecToLog '$PLUGINSDIR\sed.exe -f "$PLUGINSDIR\config.sed" -i-template "$PLUGINSDIR\bareos-fd.conf"'
  nsExec::ExecToLog '$PLUGINSDIR\sed.exe -f "$PLUGINSDIR\config.sed" -i-template "$PLUGINSDIR\bareos-sd.conf"'
  nsExec::ExecToLog '$PLUGINSDIR\sed.exe -f "$PLUGINSDIR\config.sed" -i-template "$PLUGINSDIR\tray-monitor.fd.conf"'
  nsExec::ExecToLog '$PLUGINSDIR\sed.exe -f "$PLUGINSDIR\config.sed" -i-template "$PLUGINSDIR\tray-monitor.fd-sd.conf"'
  nsExec::ExecToLog '$PLUGINSDIR\sed.exe -f "$PLUGINSDIR\config.sed" -i-template "$PLUGINSDIR\tray-monitor.fd-sd-dir.conf"'

  #Delete config.sed

  FileOpen $R1 $PLUGINSDIR\postgres.sed w
  FileWrite $R1 "s#XXX_REPLACE_WITH_DB_USER_XXX#$DbUser#g$\r$\n"
  FileWrite $R1 "s#XXX_REPLACE_WITH_DB_PASSWORD_XXX#with password '$DbPassword'#g$\r$\n"

  FileClose $R1
  #
  # config files for bconsole and bat to access remote director
  #
  FileOpen $R1 $PLUGINSDIR\bconsole.sed w

  FileWrite $R1 "s#XXX_REPLACE_WITH_BASENAME_XXX-dir#$DirectorName#g$\r$\n"
  FileWrite $R1 "s#XXX_REPLACE_WITH_HOSTNAME_XXX#$DirectorAddress#g$\r$\n"
  FileWrite $R1 "s#XXX_REPLACE_WITH_DIRECTOR_PASSWORD_XXX#$DirectorPassword#g$\r$\n"

  FileClose $R1


  nsExec::ExecToLog '$PLUGINSDIR\sed.exe -f "$PLUGINSDIR\bconsole.sed" -i-template "$PLUGINSDIR\bconsole.conf"'
  nsExec::ExecToLog '$PLUGINSDIR\sed.exe -f "$PLUGINSDIR\bconsole.sed" -i-template "$PLUGINSDIR\bat.conf"'

  #Delete bconsole.sed

#
#  write client config snippet for director
#
#
#  FileOpen $R1 import_this_file_into_your_director_config.txt w
#
#  FileWrite $R1 'Client {$\n'
#  FileWrite $R1 '  Name = $ClientName$\n'
#  FileWrite $R1 '  Address = $ClientAddress$\n'
#  FileWrite $R1 '  Password = "$ClientPassword"$\n'
#  FileWrite $R1 '  Catalog = "MyCatalog"$\n'
#  FileWrite $R1 '}$\n'
#
#  FileClose $R1
SectionEnd

!If ${WIN_DEBUG} == yes
# install sourcecode if WIN_DEBUG is yes
Section Sourcecode SEC_SOURCE
   SectionIn 1 2 3
   SetShellVarContext all
   SetOutPath "C:\"
   SetOverwrite ifnewer
   File /r "bareos-${VERSION}"
SectionEnd
!Endif

SubSection "File Daemon (Client)" SUBSEC_FD

Section "File Daemon and base libs" SEC_FD
SectionIn 1 2 3
  SetShellVarContext all
  # TODO: only do this if the file exists
  #  nsExec::ExecToLog '"$INSTDIR\bareos-fd.exe" /kill'
  #  sleep 3000
  #  nsExec::ExecToLog '"$INSTDIR\bareos-fd.exe" /remove'

  SetOutPath "$INSTDIR"
  SetOverwrite ifnewer
  CreateDirectory "$SMPROGRAMS\${PRODUCT_NAME}"
  CreateDirectory "$APPDATA\${PRODUCT_NAME}"
  File "bareos-fd.exe"
  File "libbareos.dll"
  File "libbareosfind.dll"
  File "libbareoslmdb.dll"
  File "libcrypto-*.dll"
  File "libgcc_s_*-1.dll"
  File "libssl-*.dll"
  File "libstdc++-6.dll"
  File "pthreadGCE2.dll"
  File "zlib1.dll"
  File "liblzo2-2.dll"
  File "libfastlz.dll"

  # for password generation
  File "openssl.exe"
  File "sed.exe"

  !insertmacro InstallConfFile bareos-fd.conf
SectionEnd

Section /o "File Daemon Plugins " SEC_FDPLUGINS
SectionIn 1 2 3
  SetShellVarContext all
  SetOutPath "$INSTDIR\Plugins"
  SetOverwrite ifnewer
  File "bpipe-fd.dll"
  File "mssqlvdi-fd.dll"
SectionEnd

Section "Open Firewall for File Daemon" SEC_FIREWALL_FD
SectionIn 1 2 3
  SetShellVarContext current
  ${If} ${AtLeastWin7}
    #
    # See http://technet.microsoft.com/de-de/library/dd734783%28v=ws.10%29.aspx
    #
    DetailPrint  "Opening Firewall, OS is Win7+"
    DetailPrint  "netsh advfirewall firewall add rule name=$\"Bareos backup client (bareos-fd) access$\" dir=in action=allow program=$\"$PROGRAMFILES64\${PRODUCT_NAME}\bareos-fd.exe$\" enable=yes protocol=TCP localport=9102 description=$\"Bareos backup client rule$\""
    # profile=[private,domain]"
    nsExec::Exec "netsh advfirewall firewall add rule name=$\"Bareos backup client (bareos-fd) access$\" dir=in action=allow program=$\"$PROGRAMFILES64\${PRODUCT_NAME}\bareos-fd.exe$\" enable=yes protocol=TCP localport=9102 description=$\"Bareos backup client rule$\""
    # profile=[private,domain]"
  ${Else}
    DetailPrint  "Opening Firewall, OS is < Win7"
    DetailPrint  "netsh firewall add portopening protocol=TCP port=9102 name=$\"Bareos backup client (bareos-fd) access$\""
    nsExec::Exec "netsh firewall add portopening protocol=TCP port=9102 name=$\"Bareos backup client (bareos-fd) access$\""
  ${EndIf}
SectionEnd



SubSectionEnd #FileDaemon Subsection


SubSection "Storage Daemon" SUBSEC_SD

Section /o "Storage Daemon" SEC_SD
SectionIn 2
  SetShellVarContext all
  SetOutPath "$INSTDIR"
  SetOverwrite ifnewer
  File "bareos-sd.exe"
  File "btape.exe"
  File "bls.exe"
  File "bextract.exe"

  CreateDirectory "C:\bareos-storage"

  !insertmacro InstallConfFile bareos-sd.conf
SectionEnd

Section /o "Storage Daemon Plugins " SEC_SDPLUGINS
SectionIn 2
  SetShellVarContext all
  SetOutPath "$INSTDIR\Plugins"
  SetOverwrite ifnewer
  File "autoxflate-sd.dll"
SectionEnd


Section "Open Firewall for Storage Daemon" SEC_FIREWALL_SD
SectionIn 2
  SetShellVarContext current
  ${If} ${AtLeastWin7}
    DetailPrint  "Opening Firewall, OS is Win7+"
    DetailPrint  "netsh advfirewall firewall add rule name=$\"Bareos storage daemon (bareos-sd) access$\" dir=in action=allow program=$\"$PROGRAMFILES64\${PRODUCT_NAME}\bareos-sd.exe$\" enable=yes protocol=TCP localport=9103 description=$\"Bareos storage daemon rule$\""
    # profile=[private,domain]"
    nsExec::Exec "netsh advfirewall firewall add rule name=$\"Bareos storage daemon (bareos-sd) access$\" dir=in action=allow program=$\"$PROGRAMFILES64\${PRODUCT_NAME}\bareos-sd.exe$\" enable=yes protocol=TCP localport=9103 description=$\"Bareos storage daemon rule$\""
    # profile=[private,domain]"
  ${Else}
    DetailPrint  "Opening Firewall, OS is < Win7"
    DetailPrint  "netsh firewall add portopening protocol=TCP port=9103 name=$\"Bareos storage daemon (bareos-sd) access$\""
    nsExec::Exec "netsh firewall add portopening protocol=TCP port=9103 name=$\"Bareos storage daemon (bareos-sd) access$\""
  ${EndIf}
SectionEnd

SubSectionEnd # Storage Daemon Subsection

SubSection "Director" SUBSEC_DIR

Section /o "Director" SEC_DIR
SectionIn 2

  SetShellVarContext all
  CreateDirectory "$APPDATA\${PRODUCT_NAME}\logs"
  CreateDirectory "$APPDATA\${PRODUCT_NAME}\working"
  CreateDirectory "$APPDATA\${PRODUCT_NAME}\scripts"
  SetOutPath "$INSTDIR"
  SetOverwrite ifnewer
  File "bareos-dir.exe"
  File "dbcheck.exe"
  File "bsmtp.exe"
  File "libbareoscats.dll"
  File "libbareoscats-postgresql.dll"

  # install sql ddl files
  # SetOutPath "$APPDATA\${PRODUCT_NAME}\scripts"

  # CopyFiles /SILENT "$PLUGINSDIR\ddl\*" "$APPDATA\${PRODUCT_NAME}\scripts"

  #  File "libbareoscats-sqlite3.dll"
  !insertmacro InstallConfFile bareos-dir.conf

  # edit sql ddl files
  nsExec::ExecToLog '$PLUGINSDIR\sed.exe -f "$PLUGINSDIR\postgres.sed" -i-template "$PLUGINSDIR\postgresql-grant.sql"'
  #nsExec::ExecToLog '$PLUGINSDIR\sed.exe -f "$PLUGINSDIR\postgres.sed" -i-template "$PLUGINSDIR\postgresql-create.sql"'
  #nsExec::ExecToLog '$PLUGINSDIR\sed.exe -f "$PLUGINSDIR\config.sed" -i-template "$PLUGINSDIR\postgresql-drop.sql"'


  # install edited sql ddl files
  Rename  "$PLUGINSDIR\postgresql-create.sql" "$APPDATA\${PRODUCT_NAME}\scripts\postgresql-create.sql"
  Rename  "$PLUGINSDIR\postgresql-grant.sql" "$APPDATA\${PRODUCT_NAME}\scripts\postgresql-grant.sql"
  Rename  "$PLUGINSDIR\postgresql-drop.sql" "$APPDATA\${PRODUCT_NAME}\scripts\postgresql-drop.sql"

  # create db-create script
  FileOpen  $R1 $PLUGINSDIR\postgresql-createdb.sql w
  FileWrite $R1 "CREATE DATABASE $DbName $DbEncoding TEMPLATE template0; $\r$\n"
  FileWrite $R1 "ALTER DATABASE $DbName SET datestyle TO 'ISO, YMD'; $\r$\n"
  FileClose $R1
  # install db-create script
  Rename  "$PLUGINSDIR\postgresql-createdb.sql" "$APPDATA\${PRODUCT_NAME}\scripts\postgresql-createdb.sql"

 # copy postgresql libs to our path

  StrCpy $R0 "$PostgresPath"
  StrCpy $R1 "\bin"

  StrCpy $PostgresBinPath "$R0$R1"
  DetailPrint "Copying dlls from $PostgresBinPath ..."

  DetailPrint "libpq.dll"
  CopyFiles /SILENT "$PostgresBinPath\libpq.dll" "$INSTDIR"

  DetailPrint "libintl*.dll"
  CopyFiles /SILENT "$PostgresBinPath\libintl*.dll" "$INSTDIR"

  DetailPrint "ssleay32.dll"
  CopyFiles /SILENT "$PostgresBinPath\ssleay32.dll" "$INSTDIR"

  DetailPrint "libeay32.dll"
  CopyFiles /SILENT "$PostgresBinPath\libeay32.dll" "$INSTDIR"

  #
  #  write database create batch file
  #
  FileOpen $R1 "$APPDATA\${PRODUCT_NAME}\scripts\postgres_db_setup.bat" w
  FileWrite $R1 'REM  call this batch file to create the bareos database in postgresql $\r$\n'
  FileWrite $R1 'REM $\r$\n'
  FileWrite $R1 'REM $\r$\n'
  FileWrite $R1 'REM  create postgresql database $\r$\n'
  FileWrite $R1 "SET PATH=%PATH%;$\"$PostgresBinPath$\"$\r$\n"
  FileWrite $R1 "cd $APPDATA\${PRODUCT_NAME}\scripts\$\r$\n"

  FileWrite $R1 "echo creating bareos database$\r$\n"
  FileWrite $R1 "psql.exe -U postgres -f postgresql-createdb.sql$\r$\n"

  FileWrite $R1 "echo creating bareos database tables$\r$\n"
  FileWrite $R1 "psql.exe -U postgres -f postgresql-create.sql $DbName$\r$\n"

  FileWrite $R1 "echo granting bareos database rights$\r$\n"
  FileWrite $R1 "psql.exe -U postgres -f postgresql-grant.sql $DbName$\r$\n"
  FileClose $R1

  #
  # write database dump file
  #
  FileOpen $R1 "$APPDATA\${PRODUCT_NAME}\scripts\make_catalog_backup.bat" w
  FileWrite $R1 '@echo off$\r$\n'
  FileWrite $R1 'REM  call this batch file to create a db dump$\r$\n'
  FileWrite $R1 'REM  create postgresql database dump$\r$\n'
  FileWrite $R1 'SET PGUSER=bareos$\r$\n'
  FileWrite $R1 'SET PGPASSWORD=bareos$\r$\n'
  FileWrite $R1 'SET PGDATABASE=bareos$\r$\n'
  FileWrite $R1 "SET PATH=%PATH%;$\"$PostgresBinPath$\"$\r$\n"
  FileWrite $R1 'echo dumping bareos database$\r$\n'
  FileWrite $R1 "$\"$PostgresBinPath\pg_dump.exe$\" -f $\"$APPDATA\${PRODUCT_NAME}\working\bareos.sql$\" -c$\r$\n"
  FileClose $R1

  #
  # write delete sql dump file
  #
  FileOpen $R1 "$APPDATA\${PRODUCT_NAME}\scripts\delete_catalog_backup.bat" w
  FileWrite $R1 '@echo off $\r$\n'
  FileWrite $R1 'REM this script deletes the db dump $\r$\n'
  FileWrite $R1 'del $APPDATA\${PRODUCT_NAME}\working\bareos.sql $\r$\n'
  FileClose $R1


SectionEnd

Section /o "Director Plugins " SEC_DIRPLUGINS
SectionIn 2
  SetShellVarContext all
  SetOutPath "$INSTDIR\Plugins"
  SetOverwrite ifnewer
#  File "autoxflate-sd.dll"
SectionEnd

Section "Open Firewall for Director" SEC_FIREWALL_DIR
SectionIn 2
  SetShellVarContext current
  ${If} ${AtLeastWin7}
    DetailPrint  "Opening Firewall, OS is Win7+"
    DetailPrint  "netsh advfirewall firewall add rule name=$\"Bareos director (bareos-dir) access$\" dir=in action=allow program=$\"$PROGRAMFILES64\${PRODUCT_NAME}\bareos-dir.exe$\" enable=yes protocol=TCP localport=9101 description=$\"Bareos director rule$\""
    # profile=[private,domain]"
    nsExec::Exec "netsh advfirewall firewall add rule name=$\"Bareos director (bareos-dir) access$\" dir=in action=allow program=$\"$PROGRAMFILES64\${PRODUCT_NAME}\bareos-dir.exe$\" enable=yes protocol=TCP localport=9101 description=$\"Bareos director rule$\""
    # profile=[private,domain]"
  ${Else}
    DetailPrint  "Opening Firewall, OS is < Win7"
    DetailPrint  "netsh firewall add portopening protocol=TCP port=9101 name=$\"Bareos director (bareos-dir) access$\""
    nsExec::Exec "netsh firewall add portopening protocol=TCP port=9101 name=$\"Bareos director (bareos-dir) access$\""
  ${EndIf}
SectionEnd


SubSectionEnd # Director Subsection

SubSection "Consoles" SUBSEC_CONSOLES

Section /o "Text Console (bconsole)" SEC_BCONSOLE
SectionIn 2
  SetShellVarContext all
  SetOutPath "$INSTDIR"
  SetOverwrite ifnewer
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\bconsole.lnk" "$INSTDIR\bconsole.exe" '-c "$APPDATA\${PRODUCT_NAME}\bconsole.conf"'

  File "bconsole.exe"
  File "libhistory6.dll"
  File "libreadline6.dll"
  File "libtermcap-0.dll"

  !insertmacro InstallConfFile "bconsole.conf"
SectionEnd

Section /o "Tray-Monitor" SEC_TRAYMON
SectionIn 1 2
  SetShellVarContext all
  SetOutPath "$INSTDIR"
  SetOverwrite ifnewer
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\bareos-tray-monitor.lnk" "$INSTDIR\bareos-tray-monitor.exe" '-c "$APPDATA\${PRODUCT_NAME}\tray-monitor.conf"'

  # autostart
  CreateShortCut "$SMSTARTUP\bareos-tray-monitor.lnk" "$INSTDIR\bareos-tray-monitor.exe" '-c "$APPDATA\${PRODUCT_NAME}\tray-monitor.conf"'

  File "bareos-tray-monitor.exe"
  File "libpng15-15.dll"
  File "QtCore4.dll"
  File "QtGui4.dll"
  rename "$PLUGINSDIR\tray-monitor.fd.conf" "$PLUGINSDIR\tray-monitor.conf"
${If} ${SectionIsSelected} ${SEC_SD}
  delete "$PLUGINSDIR\tray-monitor.conf"
  rename "$PLUGINSDIR\tray-monitor.fd-sd.conf" "$PLUGINSDIR\tray-monitor.conf"
${EndIf}
${If} ${SectionIsSelected} ${SEC_DIR}
  delete "$PLUGINSDIR\tray-monitor.conf"
  rename "$PLUGINSDIR\tray-monitor.fd-sd-dir.conf" "$PLUGINSDIR\tray-monitor.conf"
${EndIf}

  !insertmacro InstallConfFile "tray-monitor.conf"
SectionEnd

Section /o "Qt Console (BAT)" SEC_BAT
SectionIn 2
  SetShellVarContext all
  SetOutPath "$INSTDIR"
  SetOverwrite ifnewer
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\BAT.lnk" "$INSTDIR\bat.exe" '-c "$APPDATA\${PRODUCT_NAME}\bat.conf"'
  CreateShortCut "$DESKTOP\BAT.lnk" "$INSTDIR\bat.exe" '-c "$APPDATA\${PRODUCT_NAME}\bat.conf"'

  File "bat.exe"
  File "libpng15-15.dll"
  File "QtCore4.dll"
  File "QtGui4.dll"

  !insertmacro InstallConfFile "bat.conf"
SectionEnd
SubSectionEnd # Consoles Subsection




; Section descriptions
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN

  ; FD
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_FD} "Installs the Bareos File Daemon and required Files"
  !insertmacro MUI_DESCRIPTION_TEXT ${SUBSEC_FD} "Programs belonging to the Bareos File Daemon (client)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_FDPLUGINS} "Installs the Bareos File Daemon Plugins"
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_FIREWALL_FD} "Opens the needed ports for the File Daemon in the windows firewall"

  ; SD
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_SD} "Installs the Bareos Storage Daemon"
  !insertmacro MUI_DESCRIPTION_TEXT ${SUBSEC_SD} "Programs belonging to the Bareos Storage Daemon"
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_SDPLUGINS} "Installs the Bareos Storage Daemon Plugins"
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_FIREWALL_SD} "Opens the needed ports for the Storage Daemon in the windows firewall"

  ; DIR
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_DIR} "Installs the Bareos Director Daemon"
  !insertmacro MUI_DESCRIPTION_TEXT ${SUBSEC_DIR} "Programs belonging to the Bareos Director"
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_DIRPLUGINS} "Installs the Bareos Director Plugins"
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_FIREWALL_DIR} "Opens the needed ports for the Director Daemon in the windows firewall"

  ; Consoles

  !insertmacro MUI_DESCRIPTION_TEXT ${SUBSEC_CONSOLES} "Programs to access and monitor the Bareos system (Consoles and Tray Monitor)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_BCONSOLE} "Installs the CLI client console (bconsole)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_TRAYMON} "Installs the Tray Icon to monitor the Bareos File Daemon"
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_BAT} "Installs the Qt Console (BAT)"

  ; Sourcecode
!If ${WIN_DEBUG} == yes
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_SOURCE} "Sourcecode for debugging will be installed into C:\bareos-${VERSION}"
!Endif

!insertmacro MUI_FUNCTION_DESCRIPTION_END

Section -AdditionalIcons
  SetShellVarContext all
  WriteIniStr "$INSTDIR\${PRODUCT_NAME}.url" "InternetShortcut" "URL" "${PRODUCT_WEB_SITE}"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Website.lnk" "$INSTDIR\${PRODUCT_NAME}.url"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Uninstall.lnk" "$INSTDIR\uninst.exe"
SectionEnd

Section -Post
  SetShellVarContext all
  WriteUninstaller "$INSTDIR\uninst.exe"
  WriteRegStr HKLM "${PRODUCT_DIR_REGKEY}" "" "$INSTDIR\bareos-fd.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayName" "$(^Name)"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\uninst.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayIcon" "$INSTDIR\bareos-fd.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"

  # install service
  nsExec::ExecToLog '"$INSTDIR\bareos-fd.exe" /kill'
  sleep 3000
  nsExec::ExecToLog '"$INSTDIR\bareos-fd.exe" /remove'
  nsExec::ExecToLog '"$INSTDIR\bareos-fd.exe" /install -c "$APPDATA\${PRODUCT_NAME}\bareos-fd.conf"'

  ${If} ${SectionIsSelected} ${SEC_SD}
    nsExec::ExecToLog '"$INSTDIR\bareos-sd.exe" /kill'
    sleep 3000
    nsExec::ExecToLog '"$INSTDIR\bareos-sd.exe" /remove'
    nsExec::ExecToLog '"$INSTDIR\bareos-sd.exe" /install -c "$APPDATA\${PRODUCT_NAME}\bareos-sd.conf"'
  ${EndIf}

  ${If} ${SectionIsSelected} ${SEC_DIR}
    nsExec::ExecToLog '"$INSTDIR\bareos-dir.exe" /kill'
    sleep 3000
    nsExec::ExecToLog '"$INSTDIR\bareos-dir.exe" /remove'
    nsExec::ExecToLog '"$INSTDIR\bareos-dir.exe" /install -c "$APPDATA\${PRODUCT_NAME}\bareos-dir.conf"'
  ${EndIf}
SectionEnd

Section -StartDaemon
  nsExec::ExecToLog "net start bareos-fd"

  ${If} ${SectionIsSelected} ${SEC_SD}
    nsExec::ExecToLog "net start bareos-sd"
  ${EndIf}

  ${If} ${SectionIsSelected} ${SEC_DIR}
      MessageBox MB_OK|MB_ICONINFORMATION "To setup the bareos database, please run the script$\r$\n\
                     $APPDATA\${PRODUCT_NAME}\scripts\postgres_db_setup.bat$\r$\n \
                     with administrator rights now." /SD IDOK

    nsExec::ExecToLog "net start bareos-dir"
  ${EndIf}

  ${If} ${SectionIsSelected} ${SEC_TRAYMON}
    MessageBox MB_OK|MB_ICONINFORMATION "The tray monitor will be started automatically on next login. $\r$\n Alternatively it can be started from the start menu entry now." /SD IDOK
  ${EndIf}
SectionEnd


# helper functions to find out computer name
Function GetComputerName
  Push $R0
  Push $R1
  Push $R2

  System::Call "kernel32::GetComputerNameA(t .R0, *i ${NSIS_MAX_STRLEN} R1) i.R2"

  ${StrCase} $R0 $R0 "L"

  Pop $R2
  Pop $R1
  Exch $R0
FunctionEnd

!define ComputerNameDnsFullyQualified   3

Function GetHostName
  Push $R0
  Push $R1
  Push $R2

  ${If} $OsIsNT = 1
    System::Call "kernel32::GetComputerNameExA(i ${ComputerNameDnsFullyQualified}, t .R0, *i ${NSIS_MAX_STRLEN} R1) i.R2 ?e"
    ${If} $R2 = 0
      Pop $R2
      DetailPrint "GetComputerNameExA failed - LastError = $R2"
      Call GetComputerName
      Pop $R0
    ${Else}
      Pop $R2
    ${EndIf}
  ${Else}
    Call GetComputerName
    Pop $R0
  ${EndIf}

  Pop $R2
  Pop $R1
  Exch $R0
FunctionEnd

Function .onInit
  # Check if this is Windows NT.
  StrCpy $OsIsNT 0
  ReadRegStr $R0 HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion" CurrentVersion
  ${If} $R0 != ""
    StrCpy $OsIsNT 1
  ${EndIf}

  # Check if we are installing on 64Bit, then do some settings
  ${If} ${RunningX64} # 64Bit OS
    ${If} ${BIT_WIDTH} == '32'
      MessageBox MB_OK|MB_ICONSTOP "You are running a 32 Bit installer on a 64 Bit OS.$\r$\nPlease use the 64 Bit installer."
      Abort
    ${EndIf}
    # if instdir was not altered, change installdir to  $PROGRAMFILES64\${PRODUCT_NAME}, else use what was explicitly set
    StrCmp $INSTDIR "$PROGRAMFILES\${PRODUCT_NAME}" 0 dontset64bitinstdir
    StrCpy $INSTDIR "$PROGRAMFILES64\${PRODUCT_NAME}"
    dontset64bitinstdir:
    SetRegView 64
    ${EnableX64FSRedirection}
  ${Else} # 32Bit OS
    ${If} ${BIT_WIDTH} == '64'
      MessageBox MB_OK|MB_ICONSTOP "You are running a 64 Bit installer on a 32 Bit OS.$\r$\nPlease use the 32 Bit installer."
      Abort
    ${EndIf}
  ${EndIf}

  #
  # UPGRADE: if already installed allow to uninstall installed version
  # inspired by http://nsis.sourceforge.net/Auto-uninstall_old_before_installing_new
  #
  strcpy $Upgrading "no"

  ReadRegStr $2  ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayName"
  ReadRegStr $0  ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayVersion"
  ReadRegStr $R0 ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "UninstallString"

  #
  # If there is no version installed there is not much to upgrade so jump to done.
  #
  StrCmp $R0 "" done

  #
  # As versions before 12.4.5 cannot keep the config files during silent install, we do not support upgrading here
  #
  ${VersionCompare} "12.4.5" "$0" $1
  ${select} $1
  ${case} 1
      MessageBox MB_OK|MB_ICONSTOP "Upgrade from version $0 is not supported.$\r$\nPlease uninstall and then install again."
      Abort
  ${endselect}

  strcpy $Upgrading "yes"
  ${StrRep} $INSTDIR $R0 "uninst.exe" "" # find current INSTDIR by cutting uninst.exe out of uninstall string

  MessageBox MB_OKCANCEL|MB_ICONEXCLAMATION "${PRODUCT_NAME} version $0 is already installed in $\r$\n \
         '$INSTDIR' on your system.$\r$\n$\n \
         `OK` executes the uninstaller before installing the new version$\r$\n \
         `Cancel` cancels this upgrade. $\r$\n  $\r$\n \
         It is recommended that you make a copy of your configuration files before upgrading.$\r$\n \
         " \
  /SD IDCANCEL IDOK uninst
  Abort

  ;Run the uninstaller
uninst:
  ClearErrors
  ExecWait '$R0 /S /SILENTKEEPCONFIG _?=$INSTDIR'
            ;Silent Uninstall, Keep Configuration files, Do not copy the uninstaller to a temp file

  IfErrors no_remove_uninstaller done
  # Exec $INSTDIR\uninst.exe

no_remove_uninstaller:
  MessageBox MB_OK|MB_ICONEXCLAMATION "Error during uninstall of ${PRODUCT_NAME} version $0. Aborting"
  abort

done:
# Parameters:
# Needed for Client and Tray-Mon:
#
#   Client Name
#
#   Director Name
#   Client Password
#   Client Network Address
#
#   Client Monitor Password
#
# Needed for Bconsole/Bat:
#
#   Director Network Address
#   Director Password
#

  var /GLOBAL cmdLineParams

  # Installer Options
  ${GetParameters} $cmdLineParams
  ClearErrors

  #  /? param (help)
  ClearErrors
  ${GetOptions} $cmdLineParams '/?' $R0
  IfErrors +3 0
  MessageBox MB_OK|MB_ICONINFORMATION "[/CLIENTNAME=Name of the client ressource] $\r$\n\
                    [/CLIENTPASSWORD=Password to access the client]  $\r$\n\
                    [/DIRECTORNAME=Name of Director to access the client and of the Director accessed by bconsole/BAT]  $\r$\n\
                    [/CLIENTADDRESS=Network Address of the client] $\r$\n\
                    [/CLIENTMONITORPASSWORD=Password for monitor access] $\r$\n\
                    [/CLIENTCOMPATIBLE=(0/1) client compatible setting (0=no,1=yes)]$\r$\n\
                    $\r$\n\
                    [/DIRECTORADDRESS=Network Address of the Director (for bconsole or BAT)] $\r$\n\
                    [/DIRECTORPASSWORD=Password to access Director]$\r$\n\
                    $\r$\n\
                    [/STORAGENAME=Name of the storage ressource] $\r$\n\
                    [/STORAGEPASSWORD=Password to access the storage]  $\r$\n\
                    [/STORAGEADDRESS=Network Address of the storage] $\r$\n\
                    [/STORAGEMONITORPASSWORD=Password for monitor access] $\r$\n\
                    $\r$\n\
                    [/S silent install without user interaction]$\r$\n\
                        (deletes config files on uinstall, moves existing config files away and uses newly new ones) $\r$\n\
                    [/SILENTKEEPCONFIG keep configuration files on silent uninstall and use exinsting config files during silent install]$\r$\n\
                    [/D=C:\specify\installation\directory (! HAS TO BE THE LAST OPTION !)$\r$\n\
                    [/? (this help dialog)"
#                   [/DIRECTORNAME=Name of the Director to be accessed from bconsole/BAT]"
  Abort



  ${GetOptions} $cmdLineParams "/CLIENTNAME="  $ClientName
  ClearErrors

  ${GetOptions} $cmdLineParams "/CLIENTPASSWORD=" $ClientPassword
  ClearErrors

#  ${GetOptions} $cmdLineParams "/CLIENTDIRECTORNAME=" $DirectorName
#  ClearErrors

  ${GetOptions} $cmdLineParams "/CLIENTADDRESS=" $ClientAddress
  ClearErrors

  ${GetOptions} $cmdLineParams "/CLIENTMONITORPASSWORD=" $ClientMonitorPassword
  ClearErrors

  ${GetOptions} $cmdLineParams "/CLIENTCOMPATIBLE=" $ClientCompatible
  ClearErrors

  ${GetOptions} $cmdLineParams "/DIRECTORADDRESS=" $DirectorAddress
  ClearErrors

  ${GetOptions} $cmdLineParams "/DIRECTORPASSWORD=" $DirectorPassword
  ClearErrors

  ${GetOptions} $cmdLineParams "/DIRECTORNAME=" $DirectorName
  ClearErrors


  ${GetOptions} $cmdLineParams "/STORAGENAME="  $StorageName
  ClearErrors

  ${GetOptions} $cmdLineParams "/STORAGEDAEMONNAME="  $StorageDaemonName
  ClearErrors

  ${GetOptions} $cmdLineParams "/STORAGEPASSWORD=" $StoragePassword
  ClearErrors

  ${GetOptions} $cmdLineParams "/STORAGEADDRESS=" $StorageAddress
  ClearErrors

  ${GetOptions} $cmdLineParams "/STORAGEMONITORPASSWORD=" $StorageMonitorPassword
  ClearErrors


  StrCpy $SilentKeepConfig "yes"
  ${GetOptions} $cmdLineParams "/SILENTKEEPCONFIG"  $R0
  IfErrors 0 +2         # error is set if NOT found
    StrCpy $SilentKeepConfig "no"
  ClearErrors

  InitPluginsDir
  File "/oname=$PLUGINSDIR\storagedialog.ini" "storagedialog.ini"
  File "/oname=$PLUGINSDIR\clientdialog.ini" "clientdialog.ini"
  File "/oname=$PLUGINSDIR\directordialog.ini" "directordialog.ini"
  File "/oname=$PLUGINSDIR\openssl.exe" "openssl.exe"
  File "/oname=$PLUGINSDIR\sed.exe" "sed.exe"

  # one of the two files  have to be available depending onwhat openssl Version we sue
  File /nonfatal "/oname=$PLUGINSDIR\libcrypto-8.dll" "libcrypto-8.dll"
  File /nonfatal "/oname=$PLUGINSDIR\libcrypto-10.dll" "libcrypto-10.dll"

  # Either one of this two files will be available depending on 32/64 bits.
  File /nonfatal "/oname=$PLUGINSDIR\libgcc_s_sjlj-1.dll" "libgcc_s_sjlj-1.dll"
  File /nonfatal "/oname=$PLUGINSDIR\libgcc_s_seh-1.dll" "libgcc_s_seh-1.dll"

  File /nonfatal "/oname=$PLUGINSDIR\libssl-8.dll" "libssl-8.dll"
  File /nonfatal "/oname=$PLUGINSDIR\libssl-10.dll" "libssl-10.dll"

  File "/oname=$PLUGINSDIR\libstdc++-6.dll" "libstdc++-6.dll"
  File "/oname=$PLUGINSDIR\zlib1.dll" "zlib1.dll"

  File "/oname=$PLUGINSDIR\bareos-fd.conf" "bareos-fd.conf"
  File "/oname=$PLUGINSDIR\bareos-sd.conf" "bareos-sd.conf"
  File "/oname=$PLUGINSDIR\bareos-dir.conf" "bareos-dir.conf"
  File "/oname=$PLUGINSDIR\bconsole.conf" "bconsole.conf"
  File "/oname=$PLUGINSDIR\bat.conf" "bat.conf"

  File "/oname=$PLUGINSDIR\tray-monitor.fd.conf" "tray-monitor.fd.conf"
  File "/oname=$PLUGINSDIR\tray-monitor.fd-sd.conf" "tray-monitor.fd-sd.conf"
  File "/oname=$PLUGINSDIR\tray-monitor.fd-sd-dir.conf" "tray-monitor.fd-sd-dir.conf"

  File "/oname=$PLUGINSDIR\postgresql-create.sql" ".\ddl\creates\postgresql.sql"
  File "/oname=$PLUGINSDIR\postgresql-drop.sql" ".\ddl\drops\postgresql.sql"
  File "/oname=$PLUGINSDIR\postgresql-grant.sql" ".\ddl\grants\postgresql.sql"
  # File "/oname=$PLUGINSDIR\postgresql.sql" ".\ddl\updates\postgresql.sql"



  # make first section mandatory
  SectionSetFlags ${SEC_FD} 17 # SF_SELECTED & SF_RO
  SectionSetFlags ${SEC_TRAYMON} ${SF_SELECTED} # SF_SELECTED
  SectionSetFlags ${SEC_FDPLUGINS} ${SF_SELECTED} # SF_SELECTED
  SectionSetFlags ${SEC_FIREWALL_SD} ${SF_UNSELECTED} # unselect sd firewall (is selected by default, why?)
  SectionSetFlags ${SEC_FIREWALL_DIR} ${SF_UNSELECTED} # unselect dir firewall (is selected by default, why?)

  # find out the computer name
  Call GetComputerName
  Pop $HostName

  Call GetHostName
  Pop $LocalHostAddress

  SetPluginUnload alwaysoff

  # check if password is set by cmdline. If so, skip creation
  strcmp $ClientPassword "" genclientpassword skipclientpassword
  genclientpassword:
    nsExec::Exec '"$PLUGINSDIR\openssl.exe" rand -base64 -out $PLUGINSDIR\pw.txt 33'
    pop $R0
    ${If} $R0 = 0
     FileOpen $R1 "$PLUGINSDIR\pw.txt" r
     IfErrors +4
       FileRead $R1 $R0
       ${StrTrimNewLines} $ClientPassword $R0
       FileClose $R1
    ${EndIf}
  skipclientpassword:

  strcmp $ClientMonitorPassword "" genclientmonpassword skipclientmonpassword
  genclientmonpassword:
    nsExec::Exec '"$PLUGINSDIR\openssl.exe" rand -base64 -out $PLUGINSDIR\pw.txt 33'
    pop $R0
    ${If} $R0 = 0
     FileOpen $R1 "$PLUGINSDIR\pw.txt" r
     IfErrors +4
       FileRead $R1 $R0
       ${StrTrimNewLines} $ClientMonitorPassword $R0
       FileClose $R1
    ${EndIf}
  skipclientmonpassword:


  # check if password is set by cmdline. If so, skip creation
  strcmp $StoragePassword "" genstoragepassword skipstoragepassword
  genstoragepassword:
    nsExec::Exec '"$PLUGINSDIR\openssl.exe" rand -base64 -out $PLUGINSDIR\pw.txt 33'
    pop $R0
    ${If} $R0 = 0
     FileOpen $R1 "$PLUGINSDIR\pw.txt" r
     IfErrors +4
       FileRead $R1 $R0
       ${StrTrimNewLines} $StoragePassword $R0
       FileClose $R1
    ${EndIf}
  skipstoragepassword:

  strcmp $StorageMonitorPassword "" genstoragemonpassword skipstoragemonpassword
  genstoragemonpassword:
    nsExec::Exec '"$PLUGINSDIR\openssl.exe" rand -base64 -out $PLUGINSDIR\pw.txt 33'
    pop $R0
    ${If} $R0 = 0
     FileOpen $R1 "$PLUGINSDIR\pw.txt" r
     IfErrors +4
       FileRead $R1 $R0
       ${StrTrimNewLines} $StorageMonitorPassword $R0
       FileClose $R1
    ${EndIf}
  skipstoragemonpassword:

  strcmp $DirectorPassword "" gendirectorpassword skipdirectorpassword
  gendirectorpassword:
    nsExec::Exec '"$PLUGINSDIR\openssl.exe" rand -base64 -out $PLUGINSDIR\pw.txt 33'
    pop $R0
    ${If} $R0 = 0
     FileOpen $R1 "$PLUGINSDIR\pw.txt" r
     IfErrors +4
       FileRead $R1 $R0
       ${StrTrimNewLines} $DirectorPassword $R0
       FileClose $R1
    ${EndIf}
  skipdirectorpassword:

  strcmp $DirectorMonPassword "" gendirectormonpassword skipdirectormonpassword
  gendirectormonpassword:
    nsExec::Exec '"$PLUGINSDIR\openssl.exe" rand -base64 -out $PLUGINSDIR\pw.txt 33'
    pop $R0
    ${If} $R0 = 0
     FileOpen $R1 "$PLUGINSDIR\pw.txt" r
     IfErrors +4
       FileRead $R1 $R0
       ${StrTrimNewLines} $DirectorMonPassword $R0
       FileClose $R1
    ${EndIf}
  skipdirectormonpassword:




# if the variables are not empty (because of cmdline params),
# dont set them with our own logic but leave them as they are
  strcmp $ClientName     "" +1 +2
  StrCpy $ClientName    "$HostName-fd"
  strcmp $ClientAddress "" +1 +2
  StrCpy $ClientAddress "$HostName"
  strcmp $DirectorName   "" +1 +2
  StrCpy $DirectorName  "$HostName-dir"
  strcmp $DirectorAddress  "" +1 +2
  StrCpy $DirectorAddress  "$HostName"
  strcmp $DirectorPassword "" +1 +2
  StrCpy $DirectorPassword "$DirectorPassword"

  strcmp $StorageDaemonName     "" +1 +2
  StrCpy $StorageDaemonName    "$HostName-sd"

  strcmp $StorageName     "" +1 +2
  StrCpy $StorageName    "File"

  strcmp $StorageAddress "" +1 +2
  StrCpy $StorageAddress "$HostName"

  strcmp $DbDriver "" +1 +2
  StrCpy $DbDriver "postgresql"

  strcmp $DbPassword "" +1 +2
  StrCpy $DbPassword "bareos"

  strcmp $DbPort "" +1 +2
  StrCpy $DbPort "5432"

  strcmp $DbUser "" +1 +2
  StrCpy $DbUser "bareos"

  strcmp $DbEncoding "" +1 +2
  StrCpy $DbEncoding "ENCODING 'SQL_ASCII' LC_COLLATE 'C' LC_CTYPE 'C'"

  strcmp $DbName "" +1 +2
  StrCpy $DbName "bareos"




FunctionEnd


#
## Check for Database
#
Function checkForDatabase
${IfNot} ${SectionIsSelected} ${SEC_DIR}
   goto dbcheckend
${EndIf}

# search for value of HKEY_LOCAL_MACHINE\SOFTWARE\PostgreSQL Global Development Group\PostgreSQL,  Key "Location"
   ReadRegStr $PostgresPath HKLM "SOFTWARE\PostgreSQL Global Development Group\PostgreSQL" "Location"

   StrCmp $PostgresPath "" postgresnotfound dbcheckend

postgresnotfound:

   MessageBox MB_OK|MB_ICONSTOP "Postgresql installation was not found.$\r$\nPostgresql installation is needed for the bareos director to work. $\r$\nPlease download Postgresql for windows following the instructions on$\r$\nhttp://www.postgresql.org/download/windows/. $\r$\nBareos director on windows was tested with Postgresql 9.3 "
   Quit

dbcheckend:

FunctionEnd

#
# Client Configuration Dialog
#
Function getClientParameters
push $R0
# skip if we are upgrading
strcmp $Upgrading "yes" skip

# prefill the dialog fields with our passwords and other
# information

  WriteINIStr "$PLUGINSDIR\clientdialog.ini" "Field 2" "state" $ClientName
  WriteINIStr "$PLUGINSDIR\clientdialog.ini" "Field 3" "state" $DirectorName
  WriteINIStr "$PLUGINSDIR\clientdialog.ini" "Field 4" "state" $ClientPassword
  WriteINIStr "$PLUGINSDIR\clientdialog.ini" "Field 14" "state" $ClientMonitorPassword
  WriteINIStr "$PLUGINSDIR\clientdialog.ini" "Field 5" "state" $ClientAddress
#  WriteINIStr "$PLUGINSDIR\clientdialog.ini" "Field 7" "state" "Director console password"


${If} ${SectionIsSelected} ${SEC_FD}
  InstallOptions::dialog $PLUGINSDIR\clientdialog.ini
  Pop $R0
  ReadINIStr  $ClientName              "$PLUGINSDIR\clientdialog.ini" "Field 2" "state"
  ReadINIStr  $DirectorName            "$PLUGINSDIR\clientdialog.ini" "Field 3" "state"
  ReadINIStr  $ClientPassword          "$PLUGINSDIR\clientdialog.ini" "Field 4" "state"
  ReadINIStr  $ClientMonitorPassword   "$PLUGINSDIR\clientdialog.ini" "Field 14" "state"
  ReadINIStr  $ClientAddress           "$PLUGINSDIR\clientdialog.ini" "Field 5" "state"
  ReadINIStr  $ClientCompatible        "$PLUGINSDIR\clientdialog.ini" "Field 16" "state"

${EndIf}
#  MessageBox MB_OK "Compatible:$\r$\n$Compatible"

skip:
  Pop $R0
FunctionEnd

#
# Storage Configuration Dialog
#
Function getStorageParameters
push $R0
# skip if we are upgrading
strcmp $Upgrading "yes" skip

# prefill the dialog fields with our passwords and other
# information

  WriteINIStr "$PLUGINSDIR\storagedialog.ini" "Field 2" "state" $StorageDaemonName
  WriteINIStr "$PLUGINSDIR\storagedialog.ini" "Field 3" "state" $DirectorName
  WriteINIStr "$PLUGINSDIR\storagedialog.ini" "Field 4" "state" $StoragePassword
  WriteINIStr "$PLUGINSDIR\storagedialog.ini" "Field 14" "state" $StorageMonitorPassword
  WriteINIStr "$PLUGINSDIR\storagedialog.ini" "Field 5" "state" $StorageAddress
#  WriteINIStr "$PLUGINSDIR\storagedialog.ini" "Field 7" "state" "Director console password"


${If} ${SectionIsSelected} ${SEC_SD}
  InstallOptions::dialog $PLUGINSDIR\storagedialog.ini
  Pop $R0
  ReadINIStr  $StorageDaemonName        "$PLUGINSDIR\storagedialog.ini" "Field 2" "state"
  ReadINIStr  $DirectorName             "$PLUGINSDIR\storagedialog.ini" "Field 3" "state"
  ReadINIStr  $StoragePassword          "$PLUGINSDIR\storagedialog.ini" "Field 4" "state"
  ReadINIStr  $StorageMonitorPassword   "$PLUGINSDIR\storagedialog.ini" "Field 14" "state"
  ReadINIStr  $StorageAddress           "$PLUGINSDIR\storagedialog.ini" "Field 5" "state"
${EndIf}
#  MessageBox MB_OK "$StorageName$\r$\n$StoragePassword$\r$\n$StorageMonitorPassword "

skip:
  Pop $R0
FunctionEnd

#
# Director Configuration Dialog (for bconsole and bat configuration)
#
Function getDirectorParameters
  Push $R0
strcmp $Upgrading "yes" skip
# prefill the dialog fields
  WriteINIStr "$PLUGINSDIR\directordialog.ini" "Field 2" "state" $DirectorAddress
  WriteINIStr "$PLUGINSDIR\directordialog.ini" "Field 3" "state" $DirectorPassword
#TODO: also do this if BAT is selected alone
${If} ${SectionIsSelected} ${SEC_BCONSOLE}
  InstallOptions::dialog $PLUGINSDIR\directordialog.ini
  Pop $R0
  ReadINIStr  $DirectorAddress        "$PLUGINSDIR\directordialog.ini" "Field 2" "state"
  ReadINIStr  $DirectorPassword       "$PLUGINSDIR\directordialog.ini" "Field 3" "state"

#  MessageBox MB_OK "$DirectorAddress$\r$\n$DirectorPassword"
${EndIf}
skip:
  Pop $R0
FunctionEnd

#
# Display auto-created snippet to be added to director config
#
Function displayDirconfSnippet

strcmp $Upgrading "yes" skip

# skip config snippets if we have local director
${If} ${SectionIsSelected} ${SEC_DIR}
  goto skip
${EndIf}
#
# write client config snippet for director
#
# Multiline text edits cannot be created before but have to be created at runtime.
# see http://nsis.sourceforge.net/NsDialogs_FAQ#How_to_create_a_multi-line_edit_.28text.29_control

${If} ${SectionIsSelected} ${SEC_SD} # if sd is selected, also generate sd snippet
  StrCpy $ConfigSnippet "Client {$\r$\n  \
                              Name = $ClientName$\r$\n  \
                              Address = $ClientAddress$\r$\n  \
                              Password = $\"$ClientPassword$\"$\r$\n  \
                              # uncomment the following if using bacula $\r$\n  \
                              # Catalog = $\"MyCatalog$\"$\r$\n\
                           }$\r$\n $\r$\n\
                         Storage {$\r$\n  \
                              Name = $StorageName$\r$\n  \
                              Address = $StorageAddress$\r$\n  \
                              Password = $\"$StoragePassword$\"$\r$\n  \
                              Device = FileStorage$\r$\n  \
                              Media Type = File$\r$\n\
                           }$\r$\n \
                           "
${Else}
  StrCpy $ConfigSnippet "Client {$\r$\n  \
                              Name = $ClientName$\r$\n  \
                              Address = $ClientAddress$\r$\n  \
                              Password = $\"$ClientPassword$\"$\r$\n  \
                              # uncomment the following if using bacula $\r$\n  \
                              # Catalog = $\"MyCatalog$\"$\r$\n  \
                           }$\r$\n"
${EndIf}

  nsDialogs::Create 1018
  Pop $dialog
  ${NSD_CreateGroupBox} 0 0 100% 100% "Add this configuration snippet to your Bareos Director Configuration"
  Pop $hwnd

  nsDialogs::CreateControl EDIT \
      "${__NSD_Text_STYLE}|${WS_VSCROLL}|${ES_MULTILINE}|${ES_WANTRETURN}" \
      "${__NSD_Text_EXSTYLE}" \
      10 15 95% 90% \
      $ConfigSnippet
      Pop $hwnd

  nsDialogs::Show
skip:
FunctionEnd

Function un.onUninstSuccess
  HideWindow
  MessageBox MB_ICONINFORMATION|MB_OK "$(^Name) was successfully uninstalled." /SD IDYES
FunctionEnd

Function un.onInit
  MessageBox MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2 "Do you want to uninstall $(^Name) and all its components?" /SD IDYES IDYES +2
  Abort
FunctionEnd

Section Uninstall
# UnInstaller Options
   ${GetParameters} $cmdLineParams
   ClearErrors

# check cmdline parameters
  StrCpy $SilentKeepConfig "yes"
  ${GetOptions} $cmdLineParams "/SILENTKEEPCONFIG"  $R0
  IfErrors 0 +2         # error is set if NOT found
    StrCpy $SilentKeepConfig "no"
  ClearErrors

  # on 64Bit Systems, change the INSTDIR and Registry view to remove the right entries
  ${If} ${RunningX64} # 64Bit OS
    SetRegView 64
    ${EnableX64FSRedirection}
  ${EndIf}

  SetShellVarContext all

  # uninstall service
  nsExec::ExecToLog '"$INSTDIR\bareos-fd.exe" /kill'
  sleep 3000
  nsExec::ExecToLog '"$INSTDIR\bareos-fd.exe" /remove'

  nsExec::ExecToLog '"$INSTDIR\bareos-sd.exe" /kill'
  sleep 3000
  nsExec::ExecToLog '"$INSTDIR\bareos-sd.exe" /remove'

  nsExec::ExecToLog '"$INSTDIR\bareos-dir.exe" /kill'
  sleep 3000
  nsExec::ExecToLog '"$INSTDIR\bareos-dir.exe" /remove'


  # kill tray monitor
  KillProcWMI::KillProc "bareos-tray-monitor.exe"
  # kill bconsole and bat if running
  KillProcWMI::KillProc "bconsole.exe"
  KillProcWMI::KillProc "bat.exe"

  StrCmp $SilentKeepConfig "yes" ConfDeleteSkip # keep if silent and  $SilentKeepConfig is yes

  MessageBox MB_YESNO|MB_ICONQUESTION \
    "Do you want to keep the existing configuration files?" /SD IDNO IDYES ConfDeleteSkip

  Delete "$APPDATA\${PRODUCT_NAME}\bareos-fd.conf"
  Delete "$APPDATA\${PRODUCT_NAME}\bareos-sd.conf"
  Delete "$APPDATA\${PRODUCT_NAME}\bareos-dir.conf"
  Delete "$APPDATA\${PRODUCT_NAME}\tray-monitor.conf"
  Delete "$APPDATA\${PRODUCT_NAME}\bconsole.conf"
  Delete "$APPDATA\${PRODUCT_NAME}\bat.conf"

ConfDeleteSkip:
  Delete "$APPDATA\${PRODUCT_NAME}\bareos-fd.conf.old"
  Delete "$APPDATA\${PRODUCT_NAME}\bareos-sd.conf.old"
  Delete "$APPDATA\${PRODUCT_NAME}\bareos-dir.conf.old"
  Delete "$APPDATA\${PRODUCT_NAME}\tray-monitor.conf.old"
  Delete "$APPDATA\${PRODUCT_NAME}\bconsole.conf.old"
  Delete "$APPDATA\${PRODUCT_NAME}\bat.conf.old"

  Delete "$INSTDIR\${PRODUCT_NAME}.url"
  Delete "$INSTDIR\uninst.exe"
  Delete "$INSTDIR\bareos-tray-monitor.exe"
  Delete "$INSTDIR\bat.exe"
  Delete "$INSTDIR\bareos-fd.exe"
  Delete "$INSTDIR\bareos-sd.exe"
  Delete "$INSTDIR\bareos-dir.exe"
  Delete "$INSTDIR\dbcheck.exe"
  Delete "$INSTDIR\btape.exe"
  Delete "$INSTDIR\bls.exe"
  Delete "$INSTDIR\bextract.exe"
  Delete "$INSTDIR\bconsole.exe"
  Delete "$INSTDIR\Plugins\bpipe-fd.dll"
  Delete "$INSTDIR\Plugins\mssqlvdi-fd.dll"
  Delete "$INSTDIR\Plugins\autoxflate-sd.dll"
  Delete "$INSTDIR\libbareos.dll"
  Delete "$INSTDIR\libbareosfind.dll"
  Delete "$INSTDIR\libbareoslmdb.dll"
  Delete "$INSTDIR\libbareoscats.dll"
  Delete "$INSTDIR\libbareoscats-postgresql.dll"
  Delete "$INSTDIR\libcrypto-*.dll"
  Delete "$INSTDIR\libgcc_s_*-1.dll"
  Delete "$INSTDIR\libhistory6.dll"
  Delete "$INSTDIR\libreadline6.dll"
  Delete "$INSTDIR\libssl-*.dll"
  Delete "$INSTDIR\libstdc++-6.dll"
  Delete "$INSTDIR\libtermcap-0.dll"
  Delete "$INSTDIR\pthreadGCE2.dll"
  Delete "$INSTDIR\zlib1.dll"
  Delete "$INSTDIR\QtCore4.dll"
  Delete "$INSTDIR\QtGui4.dll"
  Delete "$INSTDIR\liblzo2-2.dll"
  Delete "$INSTDIR\libfastlz.dll"
  Delete "$INSTDIR\libpng15-15.dll"
  Delete "$INSTDIR\openssl.exe"
  Delete "$INSTDIR\sed.exe"

  Delete "$INSTDIR\bsmtp.exe"
  Delete "$INSTDIR\*template"

# copied stuff from postgresql install
  Delete "$INSTDIR\libpq.dll"
  Delete "$INSTDIR\libintl*.dll"
  Delete "$INSTDIR\ssleay32.dll"
  Delete "$INSTDIR\libeay32.dll"

# batch scripts and sql files
  Delete "$APPDATA\${PRODUCT_NAME}\scripts\*.bat"
  Delete "$APPDATA\${PRODUCT_NAME}\scripts\*.sql"
# log
  Delete "$APPDATA\${PRODUCT_NAME}\logs\*.log"

  RMDir "$APPDATA\${PRODUCT_NAME}\logs"
  RMDir "$APPDATA\${PRODUCT_NAME}\working"
  RMDir "$APPDATA\${PRODUCT_NAME}\scripts"
  RMDir  "$APPDATA\${PRODUCT_NAME}"



  Delete "$SMPROGRAMS\${PRODUCT_NAME}\Uninstall.lnk"
  Delete "$SMPROGRAMS\${PRODUCT_NAME}\Website.lnk"
  Delete "$DESKTOP\${PRODUCT_NAME}.lnk"
  Delete "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME}.lnk"

  # shortcuts
  Delete "$SMPROGRAMS\${PRODUCT_NAME}\Edit*.lnk"
  Delete "$SMPROGRAMS\${PRODUCT_NAME}\bconsole.lnk"
  Delete "$SMPROGRAMS\${PRODUCT_NAME}\BAT.lnk"
  Delete "$DESKTOP\BAT.lnk"
  # traymon
  Delete "$SMPROGRAMS\${PRODUCT_NAME}\bareos-tray-monitor.lnk"
  # traymon autostart
  Delete "$SMSTARTUP\bareos-tray-monitor.lnk"

  Delete "$SMPROGRAMS\${PRODUCT_NAME}\Website.lnk"
  Delete "$SMPROGRAMS\${PRODUCT_NAME}\Uninstall.lnk"
  RMDir "$SMPROGRAMS\${PRODUCT_NAME}"
  RMDir "$INSTDIR\Plugins"
  RMDir "$INSTDIR"
  DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}"
  DeleteRegKey HKLM "${PRODUCT_DIR_REGKEY}"
  SetAutoClose true

  # close Firewall
  ${If} ${AtLeastWin7}
    DetailPrint  "Closing Firewall, OS is Win7+"
    DetailPrint  "netsh advfirewall firewall delete rule name=$\"Bareos backup client (bareos-fd) access$\""
    nsExec::Exec "netsh advfirewall firewall delete rule name=$\"Bareos backup client (bareos-fd) access$\""
    DetailPrint  "netsh advfirewall firewall delete rule name=$\"Bareos storage daemon (bareos-sd) access$\""
    nsExec::Exec "netsh advfirewall firewall delete rule name=$\"Bareos storage daemon (bareos-sd) access$\""
    DetailPrint  "netsh advfirewall firewall delete rule name=$\"Bareos director (bareos-dir) access$\""
    nsExec::Exec "netsh advfirewall firewall delete rule name=$\"Bareos director (bareos-dir) access$\""

  ${Else}
    DetailPrint  "Closing Firewall, OS is < Win7"
    DetailPrint  "netsh firewall delete portopening protocol=TCP port=9102 name=$\"Bareos backup client (bareos-fd) access$\""
    nsExec::Exec "netsh firewall delete portopening protocol=TCP port=9102 name=$\"Bareos backup client (bareos-fd) access$\""
    DetailPrint  "netsh firewall delete portopening protocol=TCP port=9103 name=$\"Bareos storage dameon (bareos-sd) access$\""
    nsExec::Exec "netsh firewall delete portopening protocol=TCP port=9103 name=$\"Bareos storage daemon (bareos-sd) access$\""
    DetailPrint  "netsh firewall delete portopening protocol=TCP port=9101 name=$\"Bareos director (bareos-dir) access$\""
    nsExec::Exec "netsh firewall delete portopening protocol=TCP port=9101 name=$\"Bareos director (bareos-dir) access$\""

  ${EndIf}

  # remove sourcecode
  RMDir /r "C:\bareos-${VERSION}"

SectionEnd

Function .onSelChange
Push $R0
Push $R1

  # Check if BAT was just selected then select SEC_BCONSOLE
  SectionGetFlags ${SEC_BAT} $R0
  IntOp $R0 $R0 & ${SF_SELECTED}
  StrCmp $R0 ${SF_SELECTED} 0 +2
  SectionSetFlags ${SEC_BCONSOLE} $R0
Pop $R1
Pop $R0
FunctionEnd
