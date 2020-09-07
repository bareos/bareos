;
;   BAREOS - Backup Archiving REcovery Open Sourced
;
;   Copyright (C) 2012-2020 Bareos GmbH & Co. KG
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

#SilentInstall silentlog

!define SF_UNSELECTED   0

BrandingText "Bareos Installer"

; HM NIS Edit Wizard helper defines
!define PRODUCT_NAME "Bareos"
#!define PRODUCT_VERSION "1.0"
!define PRODUCT_PUBLISHER "Bareos GmbH & Co.KG"
!define PRODUCT_WEB_SITE "http://www.bareos.com"
!define PRODUCT_DIR_REGKEY "Software\Microsoft\Windows\CurrentVersion\App Paths\bareos-fd.exe"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"
!define INSTALLER_HELP "[/S silent install]$\r$\n\
[/SILENTKEEPCONFIG keep config on silent upgrades]$\r$\n\
[/WRITELOGS log to INSTDIR\install.log]$\r$\n\
[/D=installation directory (HAS TO BE THE LAST OPTION !)]$\r$\n\
$\r$\n\
[/CLIENTNAME=name]$\r$\n\
[/CLIENTPASSWORD=password]$\r$\n\
[/CLIENTADDRESS=network address]$\r$\n\
[/CLIENTMONITORPASSWORD=password]$\r$\n\
$\r$\n\
[/DIRECTORADDRESS=network address]$\r$\n\
[/DIRECTORNAME=name]$\r$\n\
[/DIRECTORPASSWORD=password]$\r$\n\
$\r$\n\
[/INSTALLDIRECTOR]$\r$\n\
[/DBDRIVER=database driver <postgresql (default)|sqlite3>]$\r$\n\
[/DBADMINUSER=database user (only Postgres)]$\r$\n\
[/DBADMINPASSWORD=database password (only Postgres)]$\r$\n\
[/INSTALLWEBUI requires 'Visual C++ Redistributable for Visual Studio 2012 x86', sets /INSTALLDIRECTOR]$\r$\n\
[/WEBUILISTENADDRESS=network address, default 127.0.0.1]$\r$\n\
[/WEBUILISTENPORT=network port, default 9100]$\r$\n\
[/WEBUILOGIN=login name, default admin]$\r$\n\
[/WEBUIPASSWORD=password, default admin]$\r$\n\
$\r$\n\
[/INSTALLSTORAGE]$\r$\n\
[/STORAGENAME=name]$\r$\n\
[/STORAGEPASSWORD=password]$\r$\n\
[/STORAGEADDRESS=network address]$\r$\n\
[/STORAGEMONITORPASSWORD=password]$\r$\n\
$\r$\n\
[/? (this help dialog)]"

SetCompressor /solid lzma

# variable definitions
Var LocalHostAddress
Var HostName

var SEC_DIR_POSTGRES_DESCRIPTION

# Config Parameters Dialog

# Needed for Configuring client config file
Var ClientName            #XXX_REPLACE_WITH_HOSTNAME_XXX
Var ClientPassword        #XXX_REPLACE_WITH_FD_PASSWORD_XXX
Var ClientMonitorPassword #XXX_REPLACE_WITH_FD_MONITOR_PASSWORD_XXX
Var ClientAddress         #XXX_REPLACE_WITH_FD_MONITOR_PASSWORD_XXX

# Needed for configuring the storage config file
Var StorageName        # name of the storage in the director config (Device)
Var StorageDaemonName  # name of the storage daemon
Var StoragePassword
Var StorageMonitorPassword
Var StorageAddress


# Needed for bconsole:
Var DirectorAddress       #XXX_REPLACE_WITH_HOSTNAME_XXX
Var DirectorPassword      #XXX_REPLACE_WITH_DIRECTOR_PASSWORD_XXX
Var DirectorName
Var DirectorMonPassword

# Needed for PostgreSQL Database
Var IsPostgresInstalled

Var PostgresPath
Var PostgresBinPath
var PostgresPsqlExeFullPath
Var DbDriver
Var DbPassword
Var DbPort
Var DbUser
Var DbName
Var DbEncoding
Var DbAdminPassword
Var DbAdminUser

# do we need to install Director/Storage (Cmdline setting)
Var InstallDirector
Var InstallStorage

# Install the webui (cmdline setting)
Var InstallWebUI
Var WebUIListenAddress
Var WebUIListenPort
Var WebUILogin
Var WebUIPassword

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


# variable if we do write logs or not
#
Var WriteLogs

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


; Custom page to get Client parameter
Page custom getClientParameters

; Custom page to get parameter to access the bareos-director
Page custom getDirectorParameters

; Custom page to get parameter for a local database
Page custom getDatabaseParameters getDatabaseParametersLeave

; Custom page to get parameter to for a local bareos-storage
Page custom getStorageParameters

; Instfiles page
!insertmacro MUI_PAGE_INSTFILES
; Finish page

; Custom page shows director config snippet
Page custom displayDirconfSnippet

Function LaunchLink
  StrCmp $InstallWebUI "no" skipLaunchWebui
    ExecShell "open" "http://$WebUIListenAddress:$WebUIListenPort"
  skipLaunchWebui:
  ExecShell "open" "http://www.bareos.com"
FunctionEnd

!define MUI_FINISHPAGE_RUN
#!define MUI_FINISHPAGE_RUN_NOTCHECKED
!define MUI_FINISHPAGE_RUN_TEXT "Open Bareos websites"
!define MUI_FINISHPAGE_RUN_FUNCTION "LaunchLink"
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_INSTFILES

; Language files
!insertmacro MUI_LANGUAGE "English"

; Reserve files
!insertmacro MUI_RESERVEFILE_INSTALLOPTIONS

; MUI end ------

!macro "CreateURLShortCut" "URLFile" "URLSite" "URLDesc"
  WriteINIStr "${URLFile}.URL" "InternetShortcut" "URL" "${URLSite}"
!macroend

# check if postgres is installed and set the postgres variables if so
!macro getPostgresVars

SetShellVarContext all

ClearErrors
ReadRegStr $PostgresPath HKLM "SOFTWARE\PostgreSQL Global Development Group\PostgreSQL" "Location"
${If} ${Errors}
   StrCpy $IsPostgresInstalled  "no"
${Else}
   StrCpy $IsPostgresInstalled  "yes"
   StrCpy $R0 "$PostgresPath"
   StrCpy $R1 "\bin"
   StrCpy $PostgresBinPath "$R0$R1" # create postgresbinpath
   StrCpy $PostgresPsqlExeFullPath "$\"$PostgresBinPath\psql.exe$\""
${EndIf}
!macroend


# check for the connection of the admin user with DbAdminUser/DbAdminPass
# if not successfull, abort
# this will quit the silent installer
# and jump back to the db param dialog in interactive installer
!macro CheckDbAdminConnection

!define UniqueID ${__LINE__} # create a unique Id to be able to use labels in the macro.
# See http://nsis.sourceforge.net/Tutorial:_Using_labels_in_macro%27s


    StrCmp $WriteLogs "yes" 0 +3
      LogEx::Init false $INSTDIR\sql.log
      LogEx::Write "PostgresPath=$PostgresPath"


    # set postgres port, username and password in environment
    System::Call 'kernel32::SetEnvironmentVariable(t "PGPORT", t "$DbPort")i.r0'
    System::Call 'kernel32::SetEnvironmentVariable(t "PGUSER", t "$DbAdminUser")i.r0'
    System::Call 'kernel32::SetEnvironmentVariable(t "PGPASSWORD", t "$DbAdminPassword")i.r0'

    DetailPrint "Now trying to log into the postgres server with the DbAdmin User and Password"
    DetailPrint "Running $PostgresPsqlExeFullPath -c \copyright"
    StrCmp $WriteLogs "yes" 0 +2
       LogEx::Write "Running $PostgresPsqlExeFullPath -c \copyright"

    nsExec::Exec /TIMEOUT=10000 "$PostgresPsqlExeFullPath -c \copyright"
    Pop $0 # return value/error/timeout
    DetailPrint "Return Value is $0"
    StrCmp $WriteLogs "yes" 0 +2
       LogEx::Write "Return Value is $0"
    ${select} $0
       ${case} "1"
         DetailPrint "psql.exe was killed"
         StrCmp $WriteLogs "yes" 0 +2
            LogEx::Write "psql.exe was killed"
       ${case} "2"
         DetailPrint "connection failed, username or password wrong?"
         StrCmp $WriteLogs "yes" 0 +2
            LogEx::Write "connection failed, username or password wrong?"
       ${case} "timeout"
         DetailPrint "connection timed out, probably password is wrong?"
         StrCmp $WriteLogs "yes" 0 +2
            LogEx::Write "connection timed out, probably password is wrong?"
       ${case} "error"
         DetailPrint "could not execute $PostgresPsqlExeFullPath"
         StrCmp $WriteLogs "yes" 0 +2
            LogEx::Write "could not execute $PostgresPsqlExeFullPath"
       ${case} "0"
         DetailPrint "success"
         StrCmp $WriteLogs "yes" 0 +2
            LogEx::Write "success"
         goto afterabort_${UniqueID}
       ${caseelse}
         DetailPrint "Unknown problem executing $PostgresPsqlExeFullPath"
         StrCmp $WriteLogs "yes" 0 +2
            LogEx::Write "Unknown problem executing $PostgresPsqlExeFullPath"
    ${endselect}
   MessageBox MB_OK|MB_ICONSTOP "Connection to db server with DbAdmin credentials failed.$\r$\nplease check username/password and service$\r$\n($PostgresPsqlExeFullPath -c \copyright)" /SD IDOK
   StrCmp $WriteLogs "yes" 0 +2
      LogEx::Write "Connection to db server with DbAdmin credentials failed.$\r$\nplease check username/password and service$\r$\n($PostgresPsqlExeFullPath -c \copyright)"
   StrCmp $WriteLogs "yes" 0 +2
      LogEx::Close
      FileOpen $R1 $TEMP\abortreason.txt w
      FileWrite $R1 "database connection failed"
      FileClose $R1
   abort

afterabort_${UniqueID}:
!undef UniqueID

!macroend

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
    GOTO skipmsgbox
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

    # Set file owner to Users
    AccessControl::SetFileOwner "$APPDATA\${PRODUCT_NAME}\${fname}" "(S-1-5-32-545)"  # user
    Pop $R0
    ${If} $R0 == error
       Pop $R0
       DetailPrint `AccessControl error: $R0`
    ${EndIf}

    # Set fullaccess for Users (S-1-5-32-545)
    AccessControl::SetOnFile "$APPDATA\${PRODUCT_NAME}\${fname}" "(S-1-5-32-545)" "FullAccess"
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


!macro AllowAccessForAll fname
  # This is important to have $APPDATA variable
  # point to ProgramData folder
  # instead of current user's Roaming folder
  SetShellVarContext all

    # Disable file access inheritance
    AccessControl::DisableFileInheritance "${fname}"
    Pop $R0
    ${If} $R0 == error
       Pop $R0
       DetailPrint `AccessControl error: $R0`
    ${EndIf}

    # Set file owner to Users
    AccessControl::SetFileOwner "${fname}" "(S-1-5-32-545)"  # user
    Pop $R0
    ${If} $R0 == error
       Pop $R0
       DetailPrint `AccessControl error: $R0`
    ${EndIf}

    # Set fullaccess for Users (S-1-5-32-545)
    AccessControl::SetOnFile "${fname}" "(S-1-5-32-545)" "FullAccess"
    Pop $R0
    ${If} $R0 == error
       Pop $R0
       DetailPrint `AccessControl error: $R0`
    ${EndIf}
!macroend



Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "${PRODUCT_NAME}-${PRODUCT_VERSION}.exe"
InstallDir "$PROGRAMFILES\${PRODUCT_NAME}"
InstallDirRegKey HKLM "${PRODUCT_DIR_REGKEY}" ""
ShowInstDetails show
ShowUnInstDetails show

InstType "Standard - FileDaemon + Plugins, Traymonitor"
InstType "Full SQLite - All Daemons, Director with SQLite Backend (no DB Server needed)"
InstType "Full PostgreSQL - All Daemons,  Director PostgreSQL Backend (needs local PostgreSQL Server)"
InstType "Minimal - FileDaemon + Plugins, no Traymonitor"


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
SectionEnd


Section -SetPasswords
  SetShellVarContext all

  # TODO: replace by ConfigureConfiguration ?

  FileOpen $R1 $PLUGINSDIR\postgres.sed w
  FileWrite $R1 "s#@DB_USER@#$DbUser#g$\r$\n"
  FileWrite $R1 "s#@DB_PASS@#with password '$DbPassword'#g$\r$\n"
  FileClose $R1

  #
  # config files for bconsole to access remote director
  #
  FileOpen $R1 $PLUGINSDIR\bconsole.sed w
  FileWrite $R1 "s#@basename@-dir#$DirectorName#g$\r$\n"
  FileWrite $R1 "s#@dir_port@#9101#g$\r$\n"
  FileWrite $R1 "s#@hostname@#$DirectorAddress#g$\r$\n"
  FileWrite $R1 "s#@dir_password@#$DirectorPassword#g$\r$\n"
  FileClose $R1

  nsExec::ExecToLog '$PLUGINSDIR\sed.exe -f "$PLUGINSDIR\bconsole.sed" -i-template "$PLUGINSDIR\bconsole.conf"'
  # Configure webui

  FileOpen $R1 $PLUGINSDIR\webui.sed w

  FileWrite $R1 "s#/etc/bareos-webui/directors.ini#C:/ProgramData/Bareos/directors.ini#g$\r$\n"
  FileWrite $R1 "s#/etc/bareos-webui/configuration.ini#C:/ProgramData/Bareos/configuration.ini#g$\r$\n"
  FileWrite $R1 "s#;include_path = $\".;c.*#include_path = $\".;c:/php/includes;C:/Program Files/Bareos/bareos-webui/vendor/ZendFramework$\"#g$\r$\n"
  FileWrite $R1 "s#;extension_dir.*ext$\"#extension_dir = $\"ext$\"#g$\r$\n"
  FileWrite $R1 "s#;extension=gettext#extension=gettext#g$\r$\n"

  # set username/password to bareos/bareos
  FileWrite $R1 "s#user1#bareos#g$\r$\n"
  FileWrite $R1 "s#CHANGEME#bareos#g$\r$\n"

  # configure webui login
  #  Name = admin
  #  Password = "admin"
  #
  FileWrite $R1 "s#Name = admin#Name = $WebUILogin#g$\r$\n"
  FileWrite $R1 "s#Password = $\"admin$\"#Password = $WebUIPassword#g$\r$\n"

  FileClose $R1


  nsExec::ExecToLog '$PLUGINSDIR\sed.exe -f "$PLUGINSDIR\webui.sed" -i-template "$PLUGINSDIR\php.ini"'
  nsExec::ExecToLog '$PLUGINSDIR\sed.exe -f "$PLUGINSDIR\webui.sed" -i-template "$PLUGINSDIR\global.php"'
  nsExec::ExecToLog '$PLUGINSDIR\sed.exe -f "$PLUGINSDIR\webui.sed" -i-template "$PLUGINSDIR\admin.conf"'

SectionEnd


!If ${WIN_DEBUG} == yes
# install sourcecode if WIN_DEBUG is yes
Section Sourcecode SEC_SOURCE
   SectionIn 1 2 3 4
   SetShellVarContext all
   SetOutPath "C:\"
   SetOverwrite ifnewer
   File /r "bareos-${VERSION}"
SectionEnd
!Endif

SubSection "File Daemon (Client)" SUBSEC_FD

Section "File Daemon and base libs" SEC_FD
SectionIn 1 2 3 4
  SetShellVarContext all
  # TODO: only do this if the file exists
  #  nsExec::ExecToLog '"$INSTDIR\bareos-fd.exe" /kill'
  #  sleep 3000
  #  nsExec::ExecToLog '"$INSTDIR\bareos-fd.exe" /remove'

  SetOverwrite ifnewer
  CreateDirectory "$SMPROGRAMS\${PRODUCT_NAME}"
  CreateDirectory "$APPDATA\${PRODUCT_NAME}"
  SetOutPath "$INSTDIR"
  File bareos-config-deploy.bat
  File bareos-fd.exe
  File libbareos.dll
  File libbareosfastlz.dll
  File libbareosfind.dll
  File libbareoslmdb.dll
  File libbareossql.dll
  File libcrypto-*.dll
  File libgcc_s_*-1.dll
  File libssl-*.dll
  File libstdc++-6.dll
  File libwinpthread-1.dll
  File zlib1.dll
  File liblzo2-2.dll
  File libjansson-4.dll
  File iconv.dll
  File libxml2-2.dll
  File libpq.dll
  File libpcre-1.dll
  File libbz2-1.dll

  # for password generation
  File "openssl.exe"
  File "sed.exe"

  # install unittests
#  File "test_*.exe"

  # install configuration as templates
  SetOutPath "$INSTDIR\defaultconfigs\bareos-fd.d"
  File /r config\bareos-fd.d\*.*

  # install configuration as templates
  SetOutPath "$INSTDIR\defaultconfigs\tray-monitor.d\client\"
  File config\tray-monitor.d\client\FileDaemon-local.conf

  SetOutPath "$APPDATA\${PRODUCT_NAME}"
  File "config\fillup.sed"

SectionEnd



Section /o "File Daemon Plugins " SEC_FDPLUGINS
SectionIn 1 2 3 4
  SetShellVarContext all
  SetOutPath "$INSTDIR\Plugins"
  SetOverwrite ifnewer
  #File "bpipe-fd.dll"
  #File "mssqlvdi-fd.dll"
  #File "python-fd.dll"
  File "*fd*"

  File "Plugins\BareosFd*.py"
  File "Plugins\bareos-fd*.py"
SectionEnd



Section "Open Firewall for File Daemon" SEC_FIREWALL_FD
SectionIn 1 2 3 4
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
SectionIn 2 3
  SetShellVarContext all
  SetOutPath "$INSTDIR"
  SetOverwrite ifnewer
  File "libbareossd.dll"
  File "bareos-sd.exe"
  File "btape.exe"
  File "bls.exe"
  File "bextract.exe"
  File "bscan.exe"

  CreateDirectory "C:\bareos-storage"

  # install configuration as templates
  SetOutPath "$INSTDIR\defaultconfigs\bareos-sd.d"
  File /r config\bareos-sd.d\*.*

  # install configuration as templates
  SetOutPath "$INSTDIR\defaultconfigs\tray-monitor.d\storage"
  File config\tray-monitor.d\storage\StorageDaemon-local.conf

SectionEnd

Section /o "Storage Daemon Plugins " SEC_SDPLUGINS
SectionIn 2 3
  SetShellVarContext all
  SetOutPath "$INSTDIR\Plugins"
  SetOverwrite ifnewer
  File "*sd*"
  File "Plugins\BareosSd*.py"
  File "Plugins\bareos-sd*.py"
SectionEnd


Section "Open Firewall for Storage Daemon" SEC_FIREWALL_SD
SectionIn 2 3
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
SectionIn 2 3

  SetShellVarContext all
  CreateDirectory "$APPDATA\${PRODUCT_NAME}\logs"
  CreateDirectory "$APPDATA\${PRODUCT_NAME}\working"
  CreateDirectory "$APPDATA\${PRODUCT_NAME}\scripts"
  SetOutPath "$INSTDIR"
  SetOverwrite ifnewer
  File "bareos-dir.exe"
  File "bareos-dbcheck.exe"
  File "bareos-dbcopy.exe"
  File "bsmtp.exe"
  File "bregex.exe"
  File "bwild.exe"
  File "libbareoscats.dll"

  # install configuration as templates
  SetOutPath "$INSTDIR\defaultconfigs\bareos-dir.d"
  File /r config\bareos-dir.d\*.*

  # install configuration as templates
  SetOutPath "$INSTDIR\defaultconfigs\tray-monitor.d\director"
  File config\tray-monitor.d\director\Director-local.conf

SectionEnd


Section /o "Director PostgreSQL Backend Support " SEC_DIR_POSTGRES
SectionIn 3
  SetShellVarContext all

  SetOutPath "$INSTDIR"
  File "libbareoscats-postgresql.dll"

  # edit sql ddl files
  nsExec::ExecToLog '$PLUGINSDIR\sed.exe -f "$PLUGINSDIR\postgres.sed" -i-template "$PLUGINSDIR\postgresql-grant.sql"'

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


  # Since PostgreSQL 9.4 unfortunately setting the PATH Variable is not enough
  # to execute psql.exe It always complains about:
  # > could not find a "psql.exe" to execute
  # > psql.exe: could not find own program executable
  # so we use the full path in the batch scripts


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
  FileWrite $R1 "$PostgresPsqlExeFullPath -f postgresql-createdb.sql$\r$\n"

  FileWrite $R1 "echo creating bareos database tables$\r$\n"
  FileWrite $R1 "$PostgresPsqlExeFullPath -f postgresql-create.sql $DbName$\r$\n"

  FileWrite $R1 "echo granting bareos database rights$\r$\n"
  FileWrite $R1 "$PostgresPsqlExeFullPath -f postgresql-grant.sql $DbName$\r$\n"
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


#
# Check if database server is installed only in silent mode
# otherwise this is done in the database dialog
#
Section -DataBaseCheck

IfSilent 0 DataBaseCheckEnd  # if we are silent, we do the db credentials check, otherwise the db dialog will do it

StrCmp $InstallDirector "no" DataBaseCheckEnd # skip DbConnection if not installing director
StrCmp $DbDriver "sqlite3" DataBaseCheckEnd # skip DbConnection if not installing director

${If} ${SectionIsSelected} ${SEC_DIR_POSTGRES}
!insertmacro CheckDbAdminConnection
${EndIF}

DataBaseCheckEnd:
SectionEnd





Section /o "Director SQLite Backend Support " SEC_DIR_SQLITE
SectionIn 2
  SetShellVarContext all

  SetOutPath "$INSTDIR"
  File "sqlite3.exe"
  File "libsqlite3-0.dll"
  File "libbareoscats-sqlite3.dll"

  Rename  "$PLUGINSDIR\sqlite3.sql" "$APPDATA\${PRODUCT_NAME}\scripts\sqlite3.sql"

  #  write database create batch file sqlite3
  #
  FileOpen $R1 "$APPDATA\${PRODUCT_NAME}\scripts\sqlite3-createdb.bat" w
  FileWrite $R1 'REM  call this batch file to create the bareos database in sqlite3 $\r$\n'
  FileWrite $R1 'REM $\r$\n'
  FileWrite $R1 'REM $\r$\n'
  FileWrite $R1 'REM  create sqlite3 database $\r$\n'

  FileWrite $R1 "cd $APPDATA\${PRODUCT_NAME}\scripts\$\r$\n"

  FileWrite $R1 "echo creating bareos database$\r$\n"
  FileWrite $R1 "$\"$INSTDIR\sqlite3.exe$\" $\"$APPDATA\${PRODUCT_NAME}\\working\bareos.db$\" < $\"$APPDATA\${PRODUCT_NAME}\scripts\sqlite3.sql$\"$\r$\n"
  FileClose $R1

  #
  # write database dump file
  #
  FileOpen $R1 "$APPDATA\${PRODUCT_NAME}\scripts\make_catalog_backup.bat" w
  FileWrite $R1 '@echo off$\r$\n'
  FileWrite $R1 'REM  call this batch file to create a db dump$\r$\n'
  FileWrite $R1 'REM  create sqlite3 database dump$\r$\n'
  FileWrite $R1 'echo dumping bareos database$\r$\n'
  FileWrite $R1 "echo .dump | $\"$INSTDIR\sqlite3.exe$\" $\"$APPDATA\${PRODUCT_NAME}\\working\bareos.db$\" > $\"$APPDATA\${PRODUCT_NAME}\working\bareos.sql$\"$\r$\n"
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


Section /o "Director Plugins" SEC_DIRPLUGINS
SectionIn 2 3
  SetShellVarContext all
  SetOutPath "$INSTDIR\Plugins"
  SetOverwrite ifnewer

  #File "python-dir.dll"
  File "*dir*"
  File "Plugins\BareosDir*.py"
  File "Plugins\bareos-dir*.py"
SectionEnd


Section "Open Firewall for Director" SEC_FIREWALL_DIR
SectionIn 2 3
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



SubSection "User Interfaces" SUBSEC_CONSOLES

Section /o "Tray-Monitor" SEC_TRAYMON
SectionIn 1 2 3
  SetShellVarContext all
  SetOutPath "$INSTDIR"
  SetOverwrite ifnewer
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\bareos-tray-monitor.lnk" "$INSTDIR\bareos-tray-monitor.exe"

  # autostart
  CreateShortCut "$SMSTARTUP\bareos-tray-monitor.lnk" "$INSTDIR\bareos-tray-monitor.exe"

  File "bareos-tray-monitor.exe"
  File "libpng*.dll"
  File "Qt5Core.dll"
  File "Qt5Gui.dll"
  File "Qt5Widgets.dll"
  File "icui18n65.dll"
  File "icudata65.dll"
  File "icuuc65.dll"
  File "libfreetype-6.dll"
  File "libglib-2.0-0.dll"
  File "libintl-8.dll"
  File "libharfbuzz-0.dll"
  File "libpcre2-16-0.dll"

  SetOutPath "$INSTDIR\platforms"
  File "qwindows.dll"


  # install configuration as templates
  SetOutPath "$INSTDIR\defaultconfigs\tray-monitor.d\monitor"
  File config\tray-monitor.d\monitor\bareos-mon.conf
SectionEnd


Section "Bareos Webui" SEC_WEBUI
   SectionIn 2 3
   ; set to yes, needed for MUI_FINISHPAGE_RUN_FUNCTION
   StrCpy $InstallWebUI "yes"
   SetShellVarContext all
   SetOutPath "$INSTDIR"
   SetOverwrite ifnewer
   File /r "nssm.exe"
   File /r "bareos-webui"

IfSilent skip_vc_redist_check
   # check  for Visual C++ Redistributable für Visual Studio 2012 x86 (on 32 and 64 bit systems)
   ReadRegDword $R1 HKLM "SOFTWARE\Wow6432Node\Microsoft\VisualStudio\11.0\VC\Runtimes\x86" "Installed"
   ReadRegDword $R2 HKLM "SOFTWARE\Microsoft\VisualStudio\11.0\VC\Runtimes\x86" "Installed"
check_for_vc_redist:
   ${If} $R1 == ""
      ${If} $R2 == ""
         ExecShell "open" "https://www.microsoft.com/en-us/download/details.aspx?id=30679"
         MessageBox MB_OK|MB_ICONSTOP "Visual C++ Redistributable for Visual Studio 2012 x86 was not found$\r$\n\
                                 It is needed by the bareos-webui service.$\r$\n\
                                 Please install vcredist_x86.exe from $\r$\n\
                                 https://www.microsoft.com/en-us/download/details.aspx?id=30679$\r$\n\
                                 and click OK when done." /SD IDOK
      ${EndIf}
   ${EndIf}
   ReadRegDword $R1 HKLM "SOFTWARE\Wow6432Node\Microsoft\VisualStudio\11.0\VC\Runtimes\x86" "Installed"
   ReadRegDword $R2 HKLM "SOFTWARE\Microsoft\VisualStudio\11.0\VC\Runtimes\x86" "Installed"
   ${If} $R1 == ""
      ${If} $R2 == ""
         goto check_for_vc_redist
	  ${EndIf}
   ${EndIf}

skip_vc_redist_check:
   Rename  "$INSTDIR\bareos-webui\config\autoload\global.php" "$INSTDIR\bareos-webui\config\autoload\global.php.orig"
   Rename  "$PLUGINSDIR\global.php" "$INSTDIR\bareos-webui\config\autoload\global.php"

   Rename  "$PLUGINSDIR\php.ini"   "$APPDATA\${PRODUCT_NAME}\php.ini"
   Rename  "$PLUGINSDIR\directors.ini" "$APPDATA\${PRODUCT_NAME}\directors.ini"
   Rename  "$PLUGINSDIR\configuration.ini" "$APPDATA\${PRODUCT_NAME}\configuration.ini"


   CreateDirectory "$INSTDIR\defaultconfigs\bareos-dir.d\profile"
   Rename  "$PLUGINSDIR\webui-admin.conf" "$INSTDIR\defaultconfigs\bareos-dir.d\profile\webui-admin.conf"

   CreateDirectory "$INSTDIR\defaultconfigs\bareos-dir.d\console"
   Rename  "$PLUGINSDIR\admin.conf"       "$INSTDIR\defaultconfigs\bareos-dir.d\console\admin.conf"


   ExecWait '$INSTDIR\nssm.exe install bareos-webui $INSTDIR\bareos-webui\php\php.exe'
   ExecWait '$INSTDIR\nssm.exe set     bareos-webui AppDirectory \"$INSTDIR\bareos-webui\"'
   ExecWait '$INSTDIR\nssm.exe set     bareos-webui Application  $INSTDIR\bareos-webui\php\php.exe'
   ExecWait '$INSTDIR\nssm.exe set     bareos-webui AppEnvironmentExtra BAREOS_WEBUI_CONFDIR=$APPDATA\${PRODUCT_NAME}\'

   # nssm.exe wants """ """ around parameters with spaces, the executable itself without quotes
   # see https://nssm.cc/usage -> quoting issues
   ExecWait '$INSTDIR\nssm.exe set bareos-webui AppParameters \
      -S $WebUIListenAddress:$WebUIListenPort \
      -c $\"$\"$\"$APPDATA\${PRODUCT_NAME}\php.ini$\"$\"$\" \
      -t $\"$\"$\"$INSTDIR\bareos-webui\public$\"$\"$\"'
   ExecWait '$INSTDIR\nssm.exe set bareos-webui AppStdout $\"$\"$\"$APPDATA\${PRODUCT_NAME}\logs\bareos-webui.log$\"$\"$\"'
   ExecWait '$INSTDIR\nssm.exe set bareos-webui AppStderr $\"$\"$\"$APPDATA\${PRODUCT_NAME}\logs\bareos-webui.log$\"$\"$\"'

   WriteRegStr HKLM "SYSTEM\CurrentControlSet\Services\Bareos-webui" \
                     "Description" "Bareos Webui php service"

   nsExec::ExecToLog "net start Bareos-webui"

   # Shortcuts
   !insertmacro "CreateURLShortCut" "bareos-webui" "http://$WebUIListenAddress:$WebUIListenPort" "Bareos Backup Server Web Interface"
   CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\bareos-webui.lnk" "http://$WebUIListenAddress:$WebUIListenPort"

   # WebUI Firewall

   DetailPrint  "Opening Firewall for WebUI"
   DetailPrint  "netsh advfirewall firewall add rule name=$\"Bareos WebUI access$\" dir=in action=allow program=$\"$INSTDIR\bareos-webui\php\php.exe$\" enable=yes protocol=TCP localport=$WEBUILISTENPORT description=$\"Bareos WebUI rule$\""
   # profile=[private,domain]"
   nsExec::Exec "netsh advfirewall firewall add rule name=$\"Bareos WebUI access$\" dir=in action=allow program=$\"$INSTDIR\bareos-webui\php\php.exe$\" enable=yes protocol=TCP localport=$WEBUILISTENPORT description=$\"Bareos WebUI rule$\""
   # profile=[private,domain]"

SectionEnd


Section /o "Text Console (bconsole)" SEC_BCONSOLE
SectionIn 2 3
  SetShellVarContext all
  SetOutPath "$INSTDIR"
  SetOverwrite ifnewer
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\bconsole.lnk" "$INSTDIR\bconsole.exe"

  File "bconsole.exe"
  File "libhistory8.dll"
  File "libreadline8.dll"
  File "libtermcap-0.dll"

  !insertmacro InstallConfFile "bconsole.conf"
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


${If} $IsPostgresInstalled == yes
  StrCpy $SEC_DIR_POSTGRES_DESCRIPTION "PostgreSQL Catalog Database Support - Needs PostgreSQL DB Server Installation which was found in $PostgresPath"
${Else}
  StrCpy $SEC_DIR_POSTGRES_DESCRIPTION "PostgreSQL Catalog Database Support - Needs PostgreSQL DB Server Installation which was NOT found"
${EndIf}
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_DIR_POSTGRES} "$SEC_DIR_POSTGRES_DESCRIPTION"


  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_DIR_SQLITE} "SQLite Catalog Database Support - Uses integrated SQLite Support"

  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_DIRPLUGINS} "Installs the Bareos Director Plugins"
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_FIREWALL_DIR} "Opens the needed ports for the Director Daemon in the windows firewall"

  ; Consoles

  !insertmacro MUI_DESCRIPTION_TEXT ${SUBSEC_CONSOLES} "Programs to access and monitor the Bareos system (Consoles and Tray Monitor)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_BCONSOLE} "Installs the CLI client console (bconsole)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_TRAYMON} "Installs the Tray Icon to monitor the Bareos File Daemon"

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
  nsExec::ExecToLog '"$INSTDIR\bareos-fd.exe" /install'

  ${If} ${SectionIsSelected} ${SEC_SD}
    nsExec::ExecToLog '"$INSTDIR\bareos-sd.exe" /kill'
    sleep 3000
    nsExec::ExecToLog '"$INSTDIR\bareos-sd.exe" /remove'
    nsExec::ExecToLog '"$INSTDIR\bareos-sd.exe" /install'
  ${EndIf}

  ${If} ${SectionIsSelected} ${SEC_DIR}
    nsExec::ExecToLog '"$INSTDIR\bareos-dir.exe" /kill'
    sleep 3000
    nsExec::ExecToLog '"$INSTDIR\bareos-dir.exe" /remove'
    nsExec::ExecToLog '"$INSTDIR\bareos-dir.exe" /install'
  ${EndIf}
SectionEnd

Section -ConfigureConfiguration
  SetShellVarContext all

  # copy the existing template file to configure.sed
  CopyFiles "$APPDATA\${PRODUCT_NAME}\fillup.sed" "$APPDATA\${PRODUCT_NAME}\configure.sed"

  #
  # path in Bareos readable format (replace \ with /)
  #
  Var /GLOBAL BareosAppdata
  ${StrRep} '$BareosAppdata' "$APPDATA/${PRODUCT_NAME}" '\' '/'
  Var /GLOBAL BareosInstdir
  ${StrRep} '$BareosInstdir' "$INSTDIR" '\' '/'

  # open or sed file and append additional rules
  FileOpen $R1 "$APPDATA\${PRODUCT_NAME}\configure.sed" a
  # move to end of file
  FileSeek $R1 0 END

  FileWrite $R1 "s#@basename@-fd#$ClientName#g$\r$\n"
  FileWrite $R1 "s#localhost-fd#$ClientName#g$\r$\n"
  FileWrite $R1 "s#bareos-dir#$DirectorName#g$\r$\n"
  FileWrite $R1 "s#bareos-mon#$HostName-mon#g$\r$\n"
  FileWrite $R1 "s#@basename@-sd#$StorageDaemonName#g$\r$\n"

  FileWrite $R1 "s#XXX_REPLACE_WITH_DATABASE_DRIVER_XXX#$DbDriver#g$\r$\n"

  # add "Working Directory" directive
  FileWrite $R1 "s#QueryFile = #Working Directory = $\"$BareosAppdata/working$\"\n  QueryFile = #g$\r$\n"


  #
  # catalog backup scripts
  #
  FileWrite $R1 "s#make_catalog_backup.pl#make_catalog_backup.bat#g$\r$\n"
  FileWrite $R1 "s#delete_catalog_backup#delete_catalog_backup.bat#g$\r$\n"

  FileWrite $R1 "s#/tmp/bareos-restores#C:/temp/bareos-restores#g$\r$\n"

  #
  # generated by
  # find -type f -exec sed -r -n 's/.*(@.*@).*/  FileWrite $R1 "s#\1##g\$\\r\$\\n"/p' {} \; | sort | uniq
  #

  FileWrite $R1 "s#@DEFAULT_DB_TYPE@#$DbDriver#g$\r$\n"
  # FileWrite $R1 "s#@DISTVER@##g$\r$\n"
  # FileWrite $R1 "s#@TAPEDRIVE@##g$\r$\n"
  FileWrite $R1 "s#@basename@#$HostName#g$\r$\n"
  # FileWrite $R1 "s#@db_name@##g$\r$\n"
  FileWrite $R1 "s#@db_password@#$DbPassword#g$\r$\n"
  FileWrite $R1 "s#@db_port@#$DbPort#g$\r$\n"
  FileWrite $R1 "s#@db_user@#$DbUser#g$\r$\n"
  FileWrite $R1 "s#@dir_password@#$DirectorPassword#g$\r$\n"
  FileWrite $R1 "s#@fd_password@#$ClientPassword#g$\r$\n"
  FileWrite $R1 "s#@hostname@#$HostName#g$\r$\n"
  # FileWrite $R1 "s#@job_email@##g$\r$\n"
  FileWrite $R1 "s#@mon_dir_password@#$DirectorMonPassword#g$\r$\n"
  FileWrite $R1 "s#@mon_fd_password@#$ClientMonitorPassword#g$\r$\n"
  FileWrite $R1 "s#@mon_sd_password@#StorageMonitorPassword#g$\r$\n"
  FileWrite $R1 "s#@sd_password@#$StoragePassword#g$\r$\n"
  # FileWrite $R1 "s#@smtp_host@#localhost#g$\r$\n"



#  Install system binaries:      /usr/x86_64-w64-mingw32/sys-root/mingw/sbin
  FileWrite $R1 "s#/usr/.*mingw.*/sys-root/mingw/sbin#$BareosInstdir#g$\r$\n"

#  Install binaries:             /usr/x86_64-w64-mingw32/sys-root/mingw/bin
  FileWrite $R1 "s#/usr/.*mingw.*/sys-root/mingw/bin#$BareosInstdir#g$\r$\n"

#  Archive directory:            /usr/i686-w64-mingw32/sys-root/mingw/var/lib/bareos/storage
  FileWrite $R1 "s#/usr/.*mingw.*/sys-root/mingw/var/lib/bareos/storage#C:/bareos-storage#g$\r$\n"

# Log directory:                /usr/i686-w64-mingw32/sys-root/mingw/var/log/bareos
  FileWrite $R1 "s#/usr/.*mingw.*/sys-root/mingw/var/log/bareos#$BareosAppdata/logs#g$\r$\n"

#  Backend directory:            /usr/x86_64-w64-mingw32/sys-root/mingw/lib/bareos/backends
  FileWrite $R1 "s#/usr/.*mingw.*/sys-root/mingw/lib/bareos/backends#$BareosInstdir#g$\r$\n"

#  Install Bareos config dir:    /usr/x86_64-w64-mingw32/sys-root/mingw/etc/bareos
  FileWrite $R1 "s#/usr/.*mingw.*/sys-root/mingw/etc/bareos#$BareosAppdata#g$\r$\n"

#  Plugin directory:             /usr/x86_64-w64-mingw32/sys-root/mingw/lib/bareos/plugins
  FileWrite $R1 "s#/usr/.*mingw.*/sys-root/mingw/lib/bareos/plugins#$BareosInstdir/Plugins#g$\r$\n"

#  Scripts directory:            /usr/x86_64-w64-mingw32/sys-root/mingw/lib/bareos/scripts
  FileWrite $R1 "s#/usr/.*mingw.*/sys-root/mingw/lib/bareos/scripts#$BareosAppdata/scripts#g$\r$\n"

#  Working directory:            /var/lib/bareos
  FileWrite $R1 "s#/var/lib/bareos#$BareosAppdata/working#g$\r$\n"

  FileWrite $R1 "s#dbpassword = .*#dbpassword = $DbPassword#g$\r$\n"



  FileClose $R1

  nsExec::ExecToLog '"$INSTDIR\bareos-config-deploy.bat" "$INSTDIR\defaultconfigs" "$APPDATA\${PRODUCT_NAME}"'

SectionEnd


Section -StartDaemon
  nsExec::ExecToLog "net start bareos-fd"

  ${If} ${SectionIsSelected} ${SEC_SD}
    nsExec::ExecToLog "net start bareos-sd"
  ${EndIf}

  ${If} ${SectionIsSelected} ${SEC_DIR}

     ${If} $DbDriver == postgresql
       #  MessageBox MB_OK|MB_ICONINFORMATION "To setup the bareos database, please run the script$\r$\n\
       #                 $APPDATA\${PRODUCT_NAME}\scripts\postgres_db_setup.bat$\r$\n \
       #                 with administrator rights now." /SD IDOK
       LogText "### Executing $APPDATA\${PRODUCT_NAME}\scripts\postgres_db_setup.bat"
       StrCmp $WriteLogs "yes" 0 +2
          LogEx::Init false $INSTDIR\sql.log
       StrCmp $WriteLogs "yes" 0 +2
          LogEx::Write "Now executing $APPDATA\${PRODUCT_NAME}\scripts\postgres_db_setup.bat"
       nsExec::ExecToLog "$APPDATA\${PRODUCT_NAME}\scripts\postgres_db_setup.bat > $PLUGINSDIR\db_setup_output.log"
       StrCmp $WriteLogs "yes" 0 +2
          LogEx::AddFile "   >" "$PLUGINSDIR\db_setup_output.log"

     ${ElseIf} $DbDriver == sqlite3
       # create sqlite3 db
       LogText "### Executing $APPDATA\${PRODUCT_NAME}\scripts\sqlite3-createdb.bat"
       nsExec::ExecToLog "$APPDATA\${PRODUCT_NAME}\scripts\sqlite3-createdb.bat > $PLUGINSDIR\db_setup_output.log"

     ${EndIf}

      LogText "### Executing net start bareos-dir"
      nsExec::ExecToLog "net start bareos-dir"
      StrCmp $WriteLogs "yes" 0 +2
      LogEx::Close
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

  #
  # enable logging?
  #
  StrCpy $WriteLogs "yes"
  ${GetOptions} $cmdLineParams "/WRITELOGS" $R0
  IfErrors 0 +2         # error is set if NOT found
    StrCpy $WriteLogs "no"
  ClearErrors

!If ${WIN_DEBUG} == yes
    StrCpy $WriteLogs "yes"
!EndIf

  StrCmp $WriteLogs "yes" 0 +3
     LogSet on # enable nsis-own logging to $INSTDIR\install.log, needs INSTDIR defined
     LogText "Logging started, INSTDIR is $INSTDIR"

  #  /? param (help)
  ClearErrors
  ${GetOptions} $cmdLineParams '/?' $R0
  IfErrors +3 0
  MessageBox MB_OK|MB_ICONINFORMATION "${INSTALLER_HELP}"
  Abort

  # Check if this is Windows NT.
  StrCpy $OsIsNT 0
  ReadRegStr $R0 HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion" CurrentVersion
  ${If} $R0 != ""
    StrCpy $OsIsNT 1
  ${EndIf}

  # Check if we are installing on 64Bit, then do some settings
  ${If} ${RunningX64} # 64Bit OS
    LogText "Windows 64 bit"
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
    LogText "Windows 32 bit"
    ${If} ${BIT_WIDTH} == '64'
      MessageBox MB_OK|MB_ICONSTOP "You are running a 64 Bit installer on a 32 Bit OS.$\r$\nPlease use the 32 Bit installer."
      Abort
    ${EndIf}
  ${EndIf}


  !insertmacro getPostgresVars

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

  LogText "Prior Bareos version installed: $0"

  #
  # As versions before 12.4.5 cannot keep the config files during silent install, we do not support upgrading here
  #
  ${VersionCompare} "12.4.5" "$0" $1
  ${select} $1
  ${case} 1
      MessageBox MB_OK|MB_ICONSTOP "Upgrade from version $0 is not supported.$\r$\n \
         Please uninstall and then install again."
      Abort
  ${endselect}

  strcpy $Upgrading "yes"
  ${StrRep} $INSTDIR $R0 "uninst.exe" "" # find current INSTDIR by cutting uninst.exe out of uninstall string
  LogText "INSTDIR is now $INSTDIR"

  MessageBox MB_OKCANCEL|MB_ICONEXCLAMATION "${PRODUCT_NAME} version $0 is already installed in $\r$\n \
         '$INSTDIR' on your system.$\r$\n$\n \
         `OK` executes the uninstaller before installing the new version$\r$\n \
         `Cancel` cancels this upgrade. $\r$\n  $\r$\n \
         It is recommended that you make a copy of your configuration files before upgrading.$\r$\n \
         " \
  /SD IDOK IDOK uninst
  Abort

uninst:

  #
  # workaround for bug in 16.2.4 and 16.2.5-r21.1: make copy of configuration files,
  # before they are deleted by uninstaller of 16.2.4.
  #
  StrCpy $6 "$0" 6
  StrCmp $6 "16.2.4" config_backup_workaround
  StrCmp $6 "16.2.5" config_backup_workaround
  Goto no_config_backup_workaround

config_backup_workaround:
  IfFileExists "$APPDATA\${PRODUCT_NAME}\*.conf" 0 no_config_backup_workaround
    LogText "config_backup_workaround"
    CreateDirectory "$PLUGINSDIR\config-backup"
    CopyFiles "$APPDATA\${PRODUCT_NAME}\*.conf" "$PLUGINSDIR\config-backup"
  ClearErrors

no_config_backup_workaround:
  # run the uninstaller in Silent mode.
  # Keep Configuration files, Do not copy the uninstaller to a temp file.
  ExecWait '$R0 /S /SILENTKEEPCONFIG _?=$INSTDIR'
  IfErrors no_remove_uninstaller done

no_remove_uninstaller:
  MessageBox MB_OK|MB_ICONEXCLAMATION "Error during uninstall of ${PRODUCT_NAME} version $0. Aborting"
      FileOpen $R1 $TEMP\abortreason.txt w
      FileWrite $R1 "Error during uninstall of ${PRODUCT_NAME} version $0. Aborting"
      FileClose $R1
  Abort

done:
  # config backup workaround: restore config files
  IfFileExists "$PLUGINSDIR\config-backup\*.conf" 0 +3
    LogText "restore config-backup from workaround"
    CopyFiles "$PLUGINSDIR\config-backup\*.conf" "$APPDATA\${PRODUCT_NAME}"
  ClearErrors

  ${GetOptions} $cmdLineParams "/CLIENTNAME="  $ClientName
  ClearErrors

  ${GetOptions} $cmdLineParams "/CLIENTPASSWORD=" $ClientPassword
  ClearErrors

  ${GetOptions} $cmdLineParams "/CLIENTADDRESS=" $ClientAddress
  ClearErrors

  ${GetOptions} $cmdLineParams "/CLIENTMONITORPASSWORD=" $ClientMonitorPassword
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

  ${GetOptions} $cmdLineParams "/DBADMINPASSWORD=" $DbAdminPassword
  ClearErrors

  ${GetOptions} $cmdLineParams "/DBADMINUSER=" $DbAdminUser
  ClearErrors

  ${GetOptions} $cmdLineParams "/WEBUILISTENADDRESS=" $WebUIListenAddress
  ClearErrors

  ${GetOptions} $cmdLineParams "/WEBUILISTENPORT=" $WebUIListenPort
  ClearErrors

  ${GetOptions} $cmdLineParams "/WEBUILOGIN=" $WebUILogin
  ClearErrors

  ${GetOptions} $cmdLineParams "/WEBUIPASSWORD=" $WebUIPassword
  ClearErrors


  ${GetOptions} $cmdLineParams "/DBDRIVER=" $DbDriver
  ClearErrors

  strcmp $DbDriver "" +1 +2
  StrCpy $DbDriver "postgresql"

  StrCpy $InstallDirector "yes"
  ${GetOptions} $cmdLineParams "/INSTALLDIRECTOR" $R0
  IfErrors 0 +2         # error is set if NOT found
    StrCpy $InstallDirector "no"
  ClearErrors

  StrCpy $InstallWebUI "yes"
  ${GetOptions} $cmdLineParams "/INSTALLWEBUI" $R0
  IfErrors 0 +2         # error is set if NOT found
    StrCpy $InstallWebUI "no"
  ClearErrors

  StrCpy $InstallStorage "yes"
  ${GetOptions} $cmdLineParams "/INSTALLSTORAGE" $R0
  IfErrors 0 +2         # error is set if NOT found
    StrCpy $InstallStorage "no"
  ClearErrors


  StrCpy $SilentKeepConfig "yes"
  ${GetOptions} $cmdLineParams "/SILENTKEEPCONFIG" $R0
  IfErrors 0 +2         # error is set if NOT found
    StrCpy $SilentKeepConfig "no"
  ClearErrors

  InitPluginsDir
  File "/oname=$PLUGINSDIR\storagedialog.ini" "storagedialog.ini"
  File "/oname=$PLUGINSDIR\clientdialog.ini" "clientdialog.ini"
  File "/oname=$PLUGINSDIR\directordialog.ini" "directordialog.ini"
  File "/oname=$PLUGINSDIR\databasedialog.ini" "databasedialog.ini"
  File "/oname=$PLUGINSDIR\openssl.exe" "openssl.exe"
  File "/oname=$PLUGINSDIR\sed.exe" "sed.exe"

  File "/oname=$PLUGINSDIR\iconv.dll" "iconv.dll"
  File "/oname=$PLUGINSDIR\libintl-8.dll" "libintl-8.dll"
  File "/oname=$PLUGINSDIR\libwinpthread-1.dll" "libwinpthread-1.dll"

  # one of the two files  have to be available depending onwhat openssl Version we sue
  File /nonfatal "/oname=$PLUGINSDIR\libcrypto-1_1.dll" "libcrypto-1_1.dll"
  File /nonfatal "/oname=$PLUGINSDIR\libcrypto-1_1-x64.dll" "libcrypto-1_1-x64.dll"
  # Either one of this two files will be available depending on 32/64 bits.
  File /nonfatal "/oname=$PLUGINSDIR\libgcc_s_sjlj-1.dll" "libgcc_s_sjlj-1.dll"
  File /nonfatal "/oname=$PLUGINSDIR\libgcc_s_seh-1.dll" "libgcc_s_seh-1.dll"

  File /nonfatal "/oname=$PLUGINSDIR\libssl-1_1.dll" "libssl-1_1.dll"
  File /nonfatal "/oname=$PLUGINSDIR\libssl-1_1-x64.dll" "libssl-1_1-x64.dll"

  File "/oname=$PLUGINSDIR\libstdc++-6.dll" "libstdc++-6.dll"
  File "/oname=$PLUGINSDIR\zlib1.dll" "zlib1.dll"

  File "/oname=$PLUGINSDIR\bconsole.conf" "config/bconsole.conf"

  File "/oname=$PLUGINSDIR\postgresql-create.sql" ".\ddl\creates\postgresql.sql"
  File "/oname=$PLUGINSDIR\postgresql-drop.sql" ".\ddl\drops\postgresql.sql"
  File "/oname=$PLUGINSDIR\postgresql-grant.sql" ".\ddl\grants\postgresql.sql"
  # File "/oname=$PLUGINSDIR\postgresql.sql" ".\ddl\updates\postgresql.sql"

  File "/oname=$PLUGINSDIR\sqlite3.sql" ".\ddl\creates\sqlite3.sql"

  # webui
  File "/oname=$PLUGINSDIR\php.ini" ".\bareos-webui\php\php.ini"
  File "/oname=$PLUGINSDIR\global.php" ".\bareos-webui\config\autoload\global.php"
  File "/oname=$PLUGINSDIR\directors.ini" ".\bareos-webui\install\directors.ini"
  File "/oname=$PLUGINSDIR\configuration.ini" ".\bareos-webui\install\configuration.ini"
  File "/oname=$PLUGINSDIR\webui-admin.conf" ".\bareos-webui/install/bareos/bareos-dir.d/profile/webui-admin.conf"
  File "/oname=$PLUGINSDIR\admin.conf" ".\bareos-webui/install/bareos/bareos-dir.d/console/admin.conf.example"

  # make first section mandatory
  SectionSetFlags ${SEC_FD} 17 # SF_SELECTED & SF_RO
  SectionSetFlags ${SEC_TRAYMON} ${SF_SELECTED}   # traymon
  SectionSetFlags ${SEC_FDPLUGINS} ${SF_SELECTED} #  fd plugins
  SectionSetFlags ${SEC_FIREWALL_SD} ${SF_UNSELECTED} # unselect sd firewall (is selected by default, why?)
  SectionSetFlags ${SEC_FIREWALL_DIR} ${SF_UNSELECTED} # unselect dir firewall (is selected by default, why?)
  SectionSetFlags ${SEC_WEBUI} ${SF_UNSELECTED} # unselect webinterface (is selected by default, why?)

  StrCmp $InstallWebUI "no" dontInstWebUI
    SectionSetFlags ${SEC_WEBUI} ${SF_SELECTED} # webui
    StrCpy $InstallDirector "yes"               # webui needs director
  dontInstWebUI:

  StrCmp $InstallDirector "no" dontInstDir
    SectionSetFlags ${SEC_DIR} ${SF_SELECTED} # director
    SectionSetFlags ${SEC_FIREWALL_DIR} ${SF_SELECTED} # director firewall
    SectionSetFlags ${SEC_DIRPLUGINS} ${SF_SELECTED} # director plugins

    # also install bconsole if director is selected
    SectionSetFlags ${SEC_BCONSOLE} ${SF_SELECTED} # bconsole

IfSilent 0 DbDriverCheckEnd
${If} $DbDriver == sqlite3
  LogText "DbDriver is sqlite3"
  SectionSetFlags ${SEC_DIR_POSTGRES} ${SF_UNSELECTED}
  SectionSetFlags ${SEC_DIR_SQLITE} ${SF_SELECTED}
${ElseIf} $DbDriver == postgresql
  LogText "DbDriver is postgresql"
  SectionSetFlags ${SEC_DIR_POSTGRES} ${SF_SELECTED}
  SectionSetFlags ${SEC_DIR_SQLITE} ${SF_UNSELECTED}
${Else}
  LogText "DbDriver needs to be sqlite3 or postgresql but is $DbDriver"
  Abort
${EndIf}
DbDriverCheckEnd:

IfSilent AutoSelecPostgresIfAvailableEnd
${If} $IsPostgresInstalled == yes
    SectionSetFlags ${SEC_DIR_POSTGRES} ${SF_SELECTED}
    SectionSetFlags ${SEC_DIR_SQLITE} ${SF_UNSELECTED}
${Else}
    SectionSetFlags ${SEC_DIR_SQLITE} ${SF_SELECTED}
${EndIf}
AutoSelecPostgresIfAvailableEnd:

  dontInstDir:

${If} $IsPostgresInstalled == no
   SectionSetFlags ${SEC_DIR_POSTGRES} ${SF_RO}
   InstTypeSetText 2 "(!)Full PostgreSQL - All Daemons,  Director PostgreSQL Backend (needed local PostgreSQL Server missing (!)"
${EndIf}

  StrCmp $InstallStorage "no" dontInstSD
    SectionSetFlags ${SEC_SD} ${SF_SELECTED} # storage daemon
    SectionSetFlags ${SEC_FIREWALL_SD} ${SF_SELECTED} # sd firewall
    SectionSetFlags ${SEC_SDPLUGINS} ${SF_SELECTED} # sd plugins
  dontInstSD:

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
  StrCpy $DirectorName  "bareos-dir"

  strcmp $DirectorAddress  "" +1 +2
  StrCpy $DirectorAddress  "$HostName"

  strcmp $DirectorPassword "" +1 +2
  StrCpy $DirectorPassword "$DirectorPassword"

  strcmp $StorageDaemonName     "" +1 +2
  StrCpy $StorageDaemonName    "bareos-sd"

  strcmp $StorageName     "" +1 +2
  StrCpy $StorageName    "File"

  strcmp $StorageAddress "" +1 +2
  StrCpy $StorageAddress "$HostName"

  strcmp $DbPassword "" +1 +2
  StrCpy $DbPassword "bareos"

  strcmp $DbPort "" +1 +2
  StrCpy $DbPort "5432"

  strcmp $DbUser "" +1 +2
  StrCpy $DbUser "bareos"

  strcmp $WebUIListenAddress "" +1 +2
  StrCpy $WebUIListenAddress "127.0.0.1"

  strcmp $WebUIListenPort "" +1 +2
  StrCpy $WebUIListenPort "9100"

  strcmp $WebUILogin "" +1 +2
  StrCpy $WebUILogin "admin"

  strcmp $WebUIPassword "" +1 +2
  StrCpy $WebUIPassword "$\"admin$\""

  strcmp $DbEncoding "" +1 +2
  StrCpy $DbEncoding "ENCODING 'SQL_ASCII' LC_COLLATE 'C' LC_CTYPE 'C'"

  strcmp $DbName "" +1 +2
  StrCpy $DbName "bareos"

  strcmp $DbAdminUser "" +1 +2
  StrCpy $DbAdminUser "postgres"

  strcmp $DbAdminPassword "" +1 +2
  StrCpy $DbAdminPassword ""


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
  # WriteINIStr "$PLUGINSDIR\clientdialog.ini" "Field 7" "state" "Director console password"


${If} ${SectionIsSelected} ${SEC_FD}
  InstallOptions::dialog $PLUGINSDIR\clientdialog.ini
  Pop $R0
  ReadINIStr  $ClientName              "$PLUGINSDIR\clientdialog.ini" "Field 2" "state"
  ReadINIStr  $DirectorName            "$PLUGINSDIR\clientdialog.ini" "Field 3" "state"
  ReadINIStr  $ClientPassword          "$PLUGINSDIR\clientdialog.ini" "Field 4" "state"
  ReadINIStr  $ClientMonitorPassword   "$PLUGINSDIR\clientdialog.ini" "Field 14" "state"
  ReadINIStr  $ClientAddress           "$PLUGINSDIR\clientdialog.ini" "Field 5" "state"

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
  # WriteINIStr "$PLUGINSDIR\storagedialog.ini" "Field 7" "state" "Director console password"


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
# Director Configuration Dialog (for bconsole configuration)
#
Function getDirectorParameters
  Push $R0
  strcmp $Upgrading "yes" skip

  # prefill the dialog fields
  WriteINIStr "$PLUGINSDIR\directordialog.ini" "Field 2" "state" $DirectorAddress
  WriteINIStr "$PLUGINSDIR\directordialog.ini" "Field 3" "state" $DirectorPassword
${If} ${SectionIsSelected} ${SEC_BCONSOLE}
  InstallOptions::dialog $PLUGINSDIR\directordialog.ini
  Pop $R0
  ReadINIStr  $DirectorAddress        "$PLUGINSDIR\directordialog.ini" "Field 2" "state"
  ReadINIStr  $DirectorPassword       "$PLUGINSDIR\directordialog.ini" "Field 3" "state"
${EndIf}

skip:
  Pop $R0
FunctionEnd



#
# Database Configuration Dialog
#
Function getDatabaseParameters
  Push $R0

  strcmp $Upgrading "yes" skip
  strcmp $InstallDirector "no" skip

${If} ${SectionIsSelected} ${SEC_DIR_POSTGRES}
  # prefill the dialog fields
  WriteINIStr "$PLUGINSDIR\databasedialog.ini" "Field 3" "state" $DbAdminUser
  WriteINIStr "$PLUGINSDIR\databasedialog.ini" "Field 4" "state" $DbAdminPassword
  WriteINIStr "$PLUGINSDIR\databasedialog.ini" "Field 5" "state" $DbUser
  WriteINIStr "$PLUGINSDIR\databasedialog.ini" "Field 6" "state" $DbPassword
  WriteINIStr "$PLUGINSDIR\databasedialog.ini" "Field 7" "state" $DbName
  WriteINIStr "$PLUGINSDIR\databasedialog.ini" "Field 8" "state" $DbPort
  InstallOptions::dialog $PLUGINSDIR\databasedialog.ini
${EndIF}

skip:
  Pop $R0
FunctionEnd

Function getDatabaseParametersLeave
  # read values just configured
  ReadINIStr  $DbAdminUser            "$PLUGINSDIR\databasedialog.ini" "Field 3" "state"
  ReadINIStr  $DbAdminPassword        "$PLUGINSDIR\databasedialog.ini" "Field 4" "state"
  ReadINIStr  $DbUser                 "$PLUGINSDIR\databasedialog.ini" "Field 5" "state"
  ReadINIStr  $DbPassword             "$PLUGINSDIR\databasedialog.ini" "Field 6" "state"
  ReadINIStr  $DbName                 "$PLUGINSDIR\databasedialog.ini" "Field 7" "state"
  ReadINIStr  $DbPort                 "$PLUGINSDIR\databasedialog.ini" "Field 8" "state"
dbcheckend:

   StrCmp $InstallDirector "no" SkipDbCheck # skip DbConnection if not instaling director
   StrCmp $DbDriver "sqlite3" SkipDbCheck   # skip DbConnection of using sqlite3

   ${If} ${SectionIsSelected} ${SEC_DIR_POSTGRES}
     !insertmacro CheckDbAdminConnection
     MessageBox MB_OK|MB_ICONINFORMATION "Connection to db server with DbAdmin credentials was successful."
   ${EndIF}
SkipDbCheck:

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

  # kill tray monitor
  KillProcWMI::KillProc "bareos-tray-monitor.exe"
  # kill bconsole if running
  KillProcWMI::KillProc "bconsole.exe"

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

  ExecWait '$INSTDIR\nssm.exe stop bareos-webui'
  ExecWait '$INSTDIR\nssm.exe remove bareos-webui confirm'

  # be sure and also kill the other daemons
  KillProcWMI::KillProc "bareos-fd.exe"
  KillProcWMI::KillProc "bareos-sd.exe"
  KillProcWMI::KillProc "bareos-dir.exe"

  StrCmp $SilentKeepConfig "yes" ConfDeleteSkip # keep if silent and  $SilentKeepConfig is yes

  MessageBox MB_YESNO|MB_ICONQUESTION \
    "Do you want to keep the existing configuration files?" /SD IDNO IDYES ConfDeleteSkip

  Delete "$APPDATA\${PRODUCT_NAME}\bareos-fd.conf"
  RMDir /r "$APPDATA\${PRODUCT_NAME}\bareos-fd.d"
  Delete "$APPDATA\${PRODUCT_NAME}\bareos-sd.conf"
  RMDir /r "$APPDATA\${PRODUCT_NAME}\bareos-sd.d"
  Delete "$APPDATA\${PRODUCT_NAME}\bareos-dir.conf"
  RMDir /r "$APPDATA\${PRODUCT_NAME}\bareos-dir.d"
  Delete "$APPDATA\${PRODUCT_NAME}\tray-monitor.conf"
  RMDir /r "$APPDATA\${PRODUCT_NAME}\tray-monitor.d"
  Delete "$APPDATA\${PRODUCT_NAME}\bconsole.conf"
  RMDir /r "$APPDATA\${PRODUCT_NAME}\bconsole.d"

  Delete "$APPDATA\${PRODUCT_NAME}\php.ini"
  Delete "$APPDATA\${PRODUCT_NAME}\directors.ini"
  Delete "$APPDATA\${PRODUCT_NAME}\configuration.ini"
  Delete "$APPDATA\${PRODUCT_NAME}\bareos-dir.d\profile\webui-admin.conf"
  Delete "$APPDATA\${PRODUCT_NAME}\bareos-dir.d\console\admin.conf"

ConfDeleteSkip:
  # delete config files *.conf.old and *.conf.new, ...
  Delete "$APPDATA\${PRODUCT_NAME}\bareos-fd.conf.old"
  Delete "$APPDATA\${PRODUCT_NAME}\bareos-sd.conf.old"
  Delete "$APPDATA\${PRODUCT_NAME}\bareos-dir.conf.old"
  Delete "$APPDATA\${PRODUCT_NAME}\tray-monitor.conf.old"
  Delete "$APPDATA\${PRODUCT_NAME}\bconsole.conf.old"

  Delete "$APPDATA\${PRODUCT_NAME}\bareos-fd.conf.new"
  Delete "$APPDATA\${PRODUCT_NAME}\bareos-sd.conf.new"
  Delete "$APPDATA\${PRODUCT_NAME}\bareos-dir.conf.new"
  Delete "$APPDATA\${PRODUCT_NAME}\tray-monitor.conf.new"
  Delete "$APPDATA\${PRODUCT_NAME}\bconsole.conf.new"

  Delete "$INSTDIR\${PRODUCT_NAME}.url"
  Delete "$INSTDIR\uninst.exe"
  Delete "$INSTDIR\bareos-tray-monitor.exe"
  Delete "$INSTDIR\bareos-fd.exe"
  Delete "$INSTDIR\bareos-sd.exe"
  Delete "$INSTDIR\bareos-dir.exe"
  Delete "$INSTDIR\bareos-dbcheck.exe"
  Delete "$INSTDIR\bareos-dbcopy.exe"
  Delete "$INSTDIR\btape.exe"
  Delete "$INSTDIR\bls.exe"
  Delete "$INSTDIR\bextract.exe"
  Delete "$INSTDIR\bscan.exe"
  Delete "$INSTDIR\bconsole.exe"
  Delete "$INSTDIR\bareos-config-deploy.bat"

  Delete "$INSTDIR\libbareos.dll"
  Delete "$INSTDIR\libbareosfastlz.dll"
  Delete "$INSTDIR\libbareossd.dll"
  Delete "$INSTDIR\libbareosfind.dll"
  Delete "$INSTDIR\libbareoslmdb.dll"
  Delete "$INSTDIR\libbareossql.dll"
  Delete "$INSTDIR\libbareoscats.dll"
  Delete "$INSTDIR\libbareoscats-postgresql.dll"

  Delete "$INSTDIR\libbareoscats-sqlite3.dll"
  Delete "$INSTDIR\libsqlite3-0.dll"
  Delete "$INSTDIR\sqlite3.exe"

  Delete "$INSTDIR\libcrypto-*.dll"
  Delete "$INSTDIR\libgcc_s_*-1.dll"
  Delete "$INSTDIR\libhistory8.dll"
  Delete "$INSTDIR\libreadline8.dll"
  Delete "$INSTDIR\libssl-*.dll"
  Delete "$INSTDIR\libstdc++-6.dll"
  Delete "$INSTDIR\libtermcap-0.dll"
  Delete "$INSTDIR\libwinpthread-1.dll"
  Delete "$INSTDIR\zlib1.dll"
  Delete "$INSTDIR\Qt5Core.dll"
  Delete "$INSTDIR\Qt5Gui.dll"
  Delete "$INSTDIR\Qt5Widgets.dll"
  Delete "$INSTDIR\icui18n65.dll"
  Delete "$INSTDIR\icudata65.dll"
  Delete "$INSTDIR\icuuc65.dll"
  Delete "$INSTDIR\libfreetype-6.dll"
  Delete "$INSTDIR\libglib-2.0-0.dll"
  Delete "$INSTDIR\libintl-8.dll"
  Delete "$INSTDIR\libharfbuzz-0.dll"
  Delete "$INSTDIR\libpcre2-16-0.dll"
  Delete "$INSTDIR\iconv.dll"
  Delete "$INSTDIR\libxml2-2.dll"
  Delete "$INSTDIR\libpq.dll"
  Delete "$INSTDIR\libpcre-1.dll"
  Delete "$INSTDIR\libbz2-1.dll"

  RMDir /r "$INSTDIR\platforms"

  Delete "$INSTDIR\liblzo2-2.dll"
  Delete "$INSTDIR\libbareosfastlz.dll"
  Delete "$INSTDIR\libjansson-4.dll"
  Delete "$INSTDIR\libpng*.dll"
  Delete "$INSTDIR\openssl.exe"
  Delete "$INSTDIR\sed.exe"

  Delete "$INSTDIR\bsmtp.exe"
  Delete "$INSTDIR\bregex.exe"
  Delete "$INSTDIR\bwild.exe"
  Delete "$INSTDIR\*template"


# logs
  Delete "$INSTDIR\*.log"

  RMDir /r "$INSTDIR\Plugins"
  RMDir /r "$INSTDIR\defaultconfigs"

  Delete "$APPDATA\${PRODUCT_NAME}\configure.sed"
  Delete "$APPDATA\${PRODUCT_NAME}\fillup.sed"

  # batch scripts and sql files
  Delete "$APPDATA\${PRODUCT_NAME}\scripts\*.bat"
  Delete "$APPDATA\${PRODUCT_NAME}\scripts\*.sql"
  RMDir  "$APPDATA\${PRODUCT_NAME}\scripts"

  RMDir /r "$APPDATA\${PRODUCT_NAME}\logs"
  RMDir /r "$APPDATA\${PRODUCT_NAME}\working"

  # removed only if empty, to keep configs
  RMDir  "$APPDATA\${PRODUCT_NAME}"

  Delete "$SMPROGRAMS\${PRODUCT_NAME}\Uninstall.lnk"
  Delete "$SMPROGRAMS\${PRODUCT_NAME}\Website.lnk"
  Delete "$DESKTOP\${PRODUCT_NAME}.lnk"
  Delete "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME}.lnk"

  # shortcuts
  Delete "$SMPROGRAMS\${PRODUCT_NAME}\Edit*.lnk"
  Delete "$SMPROGRAMS\${PRODUCT_NAME}\bconsole.lnk"
  # traymon
  Delete "$SMPROGRAMS\${PRODUCT_NAME}\bareos-tray-monitor.lnk"
  # traymon autostart
  Delete "$SMSTARTUP\bareos-tray-monitor.lnk"

  Delete "$INSTDIR\nssm.exe"
  RMDir /r "$INSTDIR\bareos-webui"


  Delete "$SMPROGRAMS\${PRODUCT_NAME}\Website.lnk"
  Delete "$SMPROGRAMS\${PRODUCT_NAME}\Uninstall.lnk"
  RMDir  "$SMPROGRAMS\${PRODUCT_NAME}"
  RMDir  "$INSTDIR\Plugins"
  RMDir  "$INSTDIR"
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
    DetailPrint  "netsh advfirewall firewall delete rule name=$\"Bareos WebUI access$\""
    nsExec::Exec "netsh advfirewall firewall delete rule name=$\"Bareos WebUI access$\""

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

  # install log
  Delete "$INSTDIR\install.txt"
SectionEnd

Function .onSelChange
  Push $R0
  Push $R1

  # !insertmacro StartRadioButtons $1
  # !insertmacro RadioButton ${SEC_DIR_POSTGRES}
  # !insertmacro RadioButton ${SEC_DIR_SQLITE}
  # !insertmacro EndRadioButtons

  # if Postgres was not detected always disable postgresql backend

  ${If} $IsPostgresInstalled == no
    SectionSetFlags ${SEC_DIR_POSTGRES} ${SF_RO}
  ${EndIf}

  # Check if WEBUI was just selected then select SEC_DIR
  SectionGetFlags ${SEC_WEBUI} $R0
  IntOp $R0 $R0 & ${SF_SELECTED}
  StrCmp $R0 ${SF_SELECTED} 0 +2
  SectionSetFlags ${SEC_DIR} $R0

  # if director is selected, we set InstallDirector to yes and select textconsole
  StrCpy $InstallDirector "no"
  SectionGetFlags ${SEC_DIR} $R0
  IntOp $R0 $R0 & ${SF_SELECTED}
  StrCmp $R0 ${SF_SELECTED} 0 +3
  StrCpy $InstallDirector "yes"
  SectionSetFlags ${SEC_BCONSOLE} ${SF_SELECTED} # bconsole

  SectionGetFlags ${SEC_DIR_SQLITE} $R0
  IntOp $R0 $R0 & ${SF_SELECTED}
  StrCmp $R0 ${SF_SELECTED} 0 +2
  StrCpy $DbDriver "sqlite3"

  SectionGetFlags ${SEC_DIR_POSTGRES} $R0
  IntOp $R0 $R0 & ${SF_SELECTED}
  StrCmp $R0 ${SF_SELECTED} 0 +2
  StrCpy $DbDriver "postgresql"

  Pop $R1
  Pop $R0
FunctionEnd
