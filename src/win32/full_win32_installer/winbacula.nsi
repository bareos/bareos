; winbacula.nsi
;
; Began as a version written by Michel Meyers (michel@tcnnet.dyndns.org)
;
; Adapted by Kern Sibbald for native Win32 Bacula
;    added a number of elements from Christopher Hull's installer
;
; D. Scott Barninger Nov 13 2004
; added configuration editing for bconsole.conf and bwx-console.conf
; better explanation in dialog boxes for editing config files
; added Start Menu items
; fix uninstall of config files to do all not just bacula-fd.conf
;
; D. Scott Barninger Dec 05 2004
; added specification of default permissions for bacula-fd.conf
;   - thanks to Jamie Ffolliott for pointing me at cacls
; added removal of working-dir files if user selects to remove config
; uninstall is now 100% clean
;
; D. Scott Barninger Apr 17 2005
; 1.36.3 release docs update
; add pdf manual and menu shortcut
;
; Robert Nelson May 15 2006
; Added server installs and implemented Microsoft install locations
; Use LogicLib.nsh
; Added Bacula-SD and Bacula-DIR
; Replaced ParameterGiven with standard GetOptions
;
; Command line options:
;
; /service    - 
; /start
;
; netsh firewall add portopening protocol=tcp port=9102 name="Bacula-FD"


!define PRODUCT "Bacula"

;
; Include the Modern UI
;

!include "MUI.nsh"
!include "LogicLib.nsh"
!include "FileFunc.nsh"
!include "Sections.nsh"
!include "StrFunc.nsh"
!include "WinMessages.nsh"
;
; Basics
;
Name "Bacula"
OutFile "${OUT_DIR}\winbacula-${VERSION}.exe"
SetCompressor lzma
InstallDir "$PROGRAMFILES\Bacula"
InstallDirRegKey HKLM "Software\Bacula" "InstallLocation"

InstType "Client"
InstType "Server"
InstType "Full"

!insertmacro GetParent

${StrCase}
${StrRep}
${StrTok}
${StrTrimNewLines}

;
; Pull in pages
;

!define      MUI_COMPONENTSPAGE_SMALLDESC

!define      MUI_HEADERIMAGE
!define      MUI_BGCOLOR                739AB9
!define      MUI_HEADERIMAGE_BITMAP     "bacula-logo.bmp"

!InsertMacro MUI_PAGE_WELCOME
!InsertMacro MUI_PAGE_LICENSE "..\..\..\LICENSE"
Page custom EnterInstallType
!define      MUI_PAGE_CUSTOMFUNCTION_SHOW PageComponentsShow
!InsertMacro MUI_PAGE_COMPONENTS
!define      MUI_PAGE_CUSTOMFUNCTION_PRE PageDirectoryPre
!InsertMacro MUI_PAGE_DIRECTORY
Page custom EnterConfigPage1 LeaveConfigPage1
Page custom EnterConfigPage2 LeaveConfigPage2
!Define      MUI_PAGE_CUSTOMFUNCTION_LEAVE LeaveInstallPage
!InsertMacro MUI_PAGE_INSTFILES
Page custom EnterWriteTemplates
!Define      MUI_FINISHPAGE_SHOWREADME $INSTDIR\Readme.txt
!InsertMacro MUI_PAGE_FINISH

!InsertMacro MUI_UNPAGE_WELCOME
!InsertMacro MUI_UNPAGE_CONFIRM
!InsertMacro MUI_UNPAGE_INSTFILES
!InsertMacro MUI_UNPAGE_FINISH

!define      MUI_ABORTWARNING

!InsertMacro MUI_LANGUAGE "English"

!InsertMacro GetParameters
!InsertMacro GetOptions

DirText "Setup will install Bacula ${VERSION} to the directory specified below. To install in a different folder, click Browse and select another folder."

!InsertMacro MUI_RESERVEFILE_INSTALLOPTIONS
;
; Global Variables
;
Var OptService
Var OptStart
Var OptSilent

Var CommonFilesDone

Var OsIsNT

Var HostName

Var ConfigClientName
Var ConfigClientPort
Var ConfigClientMaxJobs
Var ConfigClientPassword
Var ConfigClientInstallService
Var ConfigClientStartService

Var ConfigStorageName
Var ConfigStoragePort
Var ConfigStorageMaxJobs
Var ConfigStoragePassword
Var ConfigStorageInstallService
Var ConfigStorageStartService

Var ConfigDirectorName
Var ConfigDirectorPort
Var ConfigDirectorMaxJobs
Var ConfigDirectorPassword
Var ConfigDirectorAddress
Var ConfigDirectorMailServer
Var ConfigDirectorMailAddress
Var ConfigDirectorDB
Var ConfigDirectorInstallService
Var ConfigDirectorStartService

Var ConfigMonitorName
Var ConfigMonitorPassword

Var LocalDirectorPassword
Var LocalHostAddress

Var MySQLPath
Var MySQLVersion
Var PostgreSQLPath
Var PostgreSQLVersion

Var AutomaticInstall
Var InstallType
!define NewInstall      0
!define UpgradeInstall  1
!define MigrateInstall  2

Var OldInstallDir
Var PreviousComponents
Var NewComponents

; Bit 0 = File Service
;     1 = Storage Service
;     2 = Director Service
;     3 = Command Console
;     4 = Bat Console
;     5 = wxWidgets Console
;     7 = Documentation (PDF)
;     7 = Documentation (HTML)

!define ComponentFile                   1
!define ComponentStorage                2
!define ComponentDirector               4
!define ComponentTextConsole            8
!define ComponentBatConsole             16
!define ComponentGUIConsole             32
!define ComponentPDFDocs                64
!define ComponentHTMLDocs               128

!define ComponentsRequiringUserConfig           63
!define ComponentsFileAndStorage                3
!define ComponentsFileAndStorageAndDirector     7
!define ComponentsDirectorAndTextGuiConsoles    60
!define ComponentsTextAndGuiConsoles            56

Var HDLG
Var HCTL

Function .onInit
  Push $R0
  Push $R1

  ; Process Command Line Options
  StrCpy $OptService 1
  StrCpy $OptStart 1
  StrCpy $OptSilent 0
  StrCpy $CommonFilesDone 0
  StrCpy $OsIsNT 0
  StrCpy $AutomaticInstall 0
  StrCpy $InstallType ${NewInstall}
  StrCpy $OldInstallDir ""
  StrCpy $PreviousComponents 0
  StrCpy $NewComponents 0
  StrCpy $MySQLPath ""
  StrCpy $MySQLVersion ""
  StrCpy $PostgreSQLPath ""
  StrCpy $PostgreSQLVersion ""

  ${GetParameters} $R0

  ClearErrors
  ${GetOptions} $R0 "/noservice" $R1
  IfErrors +2
    StrCpy $OptService 0

  ClearErrors
  ${GetOptions} $R0 "/nostart" $R1
  IfErrors +2
    StrCpy $OptStart 0

  IfSilent 0 +2
    StrCpy $OptSilent 1

  ReadRegStr $R0 HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion" CurrentVersion
  ${If} $R0 != ""
    StrCpy $OsIsNT 1
  ${EndIf}

  Call GetComputerName
  Pop $HostName

  Call GetHostName
  Pop $LocalHostAddress

  Call GetUserName
  Pop $ConfigDirectorMailAddress

  Call FindDatabaseApps

  ; Configuration Defaults

  StrCpy $ConfigClientName               "$HostName-fd"
  StrCpy $ConfigClientPort               9102
  StrCpy $ConfigClientMaxJobs            5
  ;StrCpy $ConfigClientPassword
  StrCpy $ConfigClientInstallService     "$OptService"
  StrCpy $ConfigClientStartService       "$OptStart"

  StrCpy $ConfigStorageName              "$HostName-sd"
  StrCpy $ConfigStoragePort              9103
  StrCpy $ConfigStorageMaxJobs           10
  ;StrCpy $ConfigStoragePassword
  StrCpy $ConfigStorageInstallService    "$OptService"
  StrCpy $ConfigStorageStartService      "$OptStart"

  ;StrCpy $ConfigDirectorName            "$HostName-dir"
  StrCpy $ConfigDirectorPort             9101
  StrCpy $ConfigDirectorMaxJobs          1
  ;StrCpy $ConfigDirectorPassword
  StrCpy $ConfigDirectorDB               0
  StrCpy $ConfigDirectorInstallService   "$OptService"
  StrCpy $ConfigDirectorStartService     "$OptStart"

  StrCpy $ConfigMonitorName              "$HostName-mon"
  ;StrCpy $ConfigMonitorPassword

  InitPluginsDir
  File "/oname=$PLUGINSDIR\openssl.exe"  "${SRC_DIR}\openssl.exe"
  File "/oname=$PLUGINSDIR\libeay32.dll" "${SRC_DIR}\libeay32.dll"
  File "/oname=$PLUGINSDIR\ssleay32.dll" "${SRC_DIR}\ssleay32.dll"
  File "/oname=$PLUGINSDIR\sed.exe"      "${SRC_DIR}\sed.exe"

  !InsertMacro MUI_INSTALLOPTIONS_EXTRACT "InstallType.ini"
  !InsertMacro MUI_INSTALLOPTIONS_EXTRACT "WriteTemplates.ini"

  SetPluginUnload alwaysoff

  nsExec::Exec '"$PLUGINSDIR\openssl.exe" rand -base64 -out $PLUGINSDIR\pw.txt 33'
  pop $R0
  ${If} $R0 = 0
   FileOpen $R1 "$PLUGINSDIR\pw.txt" r
   IfErrors +4
     FileRead $R1 $R0
     ${StrTrimNewLines} $ConfigClientPassword $R0
     FileClose $R1
  ${EndIf}

  nsExec::Exec '"$PLUGINSDIR\openssl.exe" rand -base64 -out $PLUGINSDIR\pw.txt 33'
  pop $R0
  ${If} $R0 = 0
   FileOpen $R1 "$PLUGINSDIR\pw.txt" r
   IfErrors +4
     FileRead $R1 $R0
     ${StrTrimNewLines} $ConfigStoragePassword $R0
     FileClose $R1
  ${EndIf}

  nsExec::Exec '"$PLUGINSDIR\openssl.exe" rand -base64 -out $PLUGINSDIR\pw.txt 33'
  pop $R0
  ${If} $R0 = 0
   FileOpen $R1 "$PLUGINSDIR\pw.txt" r
   IfErrors +4
     FileRead $R1 $R0
     ${StrTrimNewLines} $LocalDirectorPassword $R0
     FileClose $R1
  ${EndIf}

  SetPluginUnload manual

  nsExec::Exec '"$PLUGINSDIR\openssl.exe" rand -base64 -out $PLUGINSDIR\pw.txt 33'
  pop $R0
  ${If} $R0 = 0
   FileOpen $R1 "$PLUGINSDIR\pw.txt" r
   IfErrors +4
     FileRead $R1 $R0
     ${StrTrimNewLines} $ConfigMonitorPassword $R0
     FileClose $R1
  ${EndIf}

  Pop $R1
  Pop $R0
FunctionEnd

Function .onSelChange
  Call UpdateComponentUI
FunctionEnd

Function InstallCommonFiles
  ${If} $CommonFilesDone = 0
    SetOutPath "$INSTDIR"
    File "Readme.txt"

    SetOutPath "$INSTDIR\bin"
!if "${BUILD_TOOLS}" == "VC8"
    File "${SRC_DIR}\msvcm80.dll"
    File "${SRC_DIR}\msvcp80.dll"
    File "${SRC_DIR}\msvcr80.dll"
    File "${SRC_DIR}\Microsoft.VC80.CRT.manifest"
    File "${SRC_DIR}\pthreadVCE.dll"
!endif
!if "${BUILD_TOOLS}" == "VC8_DEBUG"
    File "${SRC_DIR}\msvcm80.dll"
    File "${SRC_DIR}\msvcp80.dll"
    File "${SRC_DIR}\msvcr80.dll"
    File "${SRC_DIR}\Microsoft.VC80.CRT.manifest"
    File "${SRC_DIR}\msvcm80d.dll"
    File "${SRC_DIR}\msvcp80d.dll"
    File "${SRC_DIR}\msvcr80d.dll"
    File "${SRC_DIR}\Microsoft.VC80.DebugCRT.manifest"
    File "${SRC_DIR}\pthreadVCE.dll"
!endif
!if "${BUILD_TOOLS}" == "MinGW"
    File "${SRC_DIR}\mingwm10.dll"
    File "${SRC_DIR}\pthreadGCE.dll"
!endif
    File "${SRC_DIR}\libeay32.dll"
    File "${SRC_DIR}\ssleay32.dll"
    File "${SRC_DIR}\zlib1.dll"
!if "${BUILD_TOOLS}" == "VC8"
    File "${SRC_DIR}\zlib1.dll.manifest"
!endif
!If "${BUILD_TOOLS}" == "VC8_DEBUG"
    File "${SRC_DIR}\zlib1.dll.manifest"
!endif
    File "/oname=$INSTDIR\openssl.cnf" "${SRC_DIR}\openssl.cnf"
    File "${SRC_DIR}\openssl.exe"
    File "${SRC_DIR}\bsleep.exe"
    File "${SRC_DIR}\bsmtp.exe"
    File "${SRC_DIR}\bacula.dll"
    File "${SRC_DIR}\expr64.exe"
    File "${SRC_DIR}\snooze.exe"

    CreateShortCut "$SMPROGRAMS\Bacula\Documentation\View Readme.lnk" "write.exe" '"$INSTDIR\Readme.txt"'

    StrCpy $CommonFilesDone 1
  ${EndIf}
FunctionEnd

Section "-Initialize"
  ${If} $MySQLPath != ""
    DetailPrint "Found MySQL (version $MySQLVersion)"
  ${EndIf}
  ${If} $PostgreSQLPath != ""
    DetailPrint "Found PostgreSQL (version $PostgreSQLVersion)"
  ${EndIf}

  WriteRegStr   HKLM Software\Bacula InstallLocation "$INSTDIR"

  Call GetSelectedComponents
  Pop $R2
  WriteRegDWORD HKLM Software\Bacula Components $R2

  WriteRegDWORD HKLM Software\Bacula Database $ConfigDirectorDB

  ; remove start menu items
  SetShellVarContext all

  Delete /REBOOTOK "$SMPROGRAMS\Bacula\Configuration\*"
  Delete /REBOOTOK "$SMPROGRAMS\Bacula\Documentation\*"
  Delete /REBOOTOK "$SMPROGRAMS\Bacula\*"
  RMDir "$SMPROGRAMS\Bacula\Configuration"
  RMDir "$SMPROGRAMS\Bacula\Documentation"
  RMDir "$SMPROGRAMS\Bacula"
  CreateDirectory "$SMPROGRAMS\Bacula"
  CreateDirectory "$SMPROGRAMS\Bacula\Configuration"
  CreateDirectory "$SMPROGRAMS\Bacula\Documentation"

  CreateDirectory "$INSTDIR"
  CreateDirectory "$INSTDIR\bin"
  CreateDirectory "$APPDATA\Bacula"
  CreateDirectory "$APPDATA\Bacula\Work"
  CreateDirectory "$APPDATA\Bacula\Spool"

  SetOutPath "$INSTDIR"
  File "..\..\..\LICENSE"
  Delete /REBOOTOK "$INSTDIR\bin\License.txt"

  FileOpen $R1 $PLUGINSDIR\config.sed w
  FileWrite $R1 "s;@VERSION@;${VERSION};g$\r$\n"
  FileWrite $R1 "s;@DATE@;${__DATE__};g$\r$\n"
  FileWrite $R1 "s;@DISTNAME@;Windows;g$\r$\n"

!If "$BUILD_TOOLS" == "MinGW"
  StrCpy $R2 "MinGW32"
!Else
  StrCpy $R2 "MVS"
!EndIf

  Call GetHostName
  Exch $R3
  Pop $R3

  FileWrite $R1 "s;@DISTVER@;$R2;g$\r$\n"

  ${StrRep} $R2 "$APPDATA\Bacula\Work" "\" "\\\\"
  FileWrite $R1 's;@working_dir@;$R2;g$\r$\n'
  ${StrRep} $R2 "$APPDATA\Bacula\Work" "\" "\\"
  FileWrite $R1 's;@working_dir_cmd@;$R2;g$\r$\n'

  ${StrRep} $R2 "$INSTDIR\bin" "\" "\\\\"
  FileWrite $R1 's;@bin_dir@;$R2;g$\r$\n'
  ${StrRep} $R2 "$INSTDIR\bin" "\" "\\"
  FileWrite $R1 's;@bin_dir_cmd@;$R2;g$\r$\n'

  ${StrRep} $R2 "$INSTDIR\bin\fdplugins" "\" "\\\\"
  FileWrite $R1 's;@fdplugins_dir@;$R2;g$\r$\n'

  ${StrRep} $R2 "$INSTDIR" "\" "/"
  FileWrite $R1 "s;@BUILD_DIR@;$R2;g$\r$\n"

  Call IsDirectorSelected
  Pop $R2
  ${If} $R2 = 1
    FileWrite $R1 "s;@director_address@;$LocalHostAddress;g$\r$\n"
  ${Else}
    ${If} "$ConfigDirectorAddress" != ""
      FileWrite $R1 "s;@director_address@;$ConfigDirectorAddress;g$\r$\n"
    ${EndIf}
  ${EndIf}

  FileWrite $R1 "s;@client_address@;$LocalHostAddress;g$\r$\n"
  FileWrite $R1 "s;@storage_address@;$LocalHostAddress;g$\r$\n"

  ${If} "$ConfigClientName" != ""
    FileWrite $R1 "s;@client_name@;$ConfigClientName;g$\r$\n"
  ${EndIf}
  ${If} "$ConfigClientPort" != ""
    FileWrite $R1 "s;@client_port@;$ConfigClientPort;g$\r$\n"
  ${EndIf}
  ${If} "$ConfigClientMaxJobs" != ""
    FileWrite $R1 "s;@client_maxjobs@;$ConfigClientMaxJobs;g$\r$\n"
  ${EndIf}
  ${If} "$ConfigClientPassword" != ""
    FileWrite $R1 "s;@client_password@;$ConfigClientPassword;g$\r$\n"
  ${EndIf}
  ${If} "$ConfigStorageName" != ""
    FileWrite $R1 "s;@storage_name@;$ConfigStorageName;g$\r$\n"
  ${EndIf}
  ${If} "$ConfigStoragePort" != ""
    FileWrite $R1 "s;@storage_port@;$ConfigStoragePort;g$\r$\n"
  ${EndIf}
  ${If} "$ConfigStorageMaxJobs" != ""
    FileWrite $R1 "s;@storage_maxjobs@;$ConfigStorageMaxJobs;g$\r$\n"
  ${EndIf}
  ${If} "$ConfigStoragePassword" != ""
    FileWrite $R1 "s;@storage_password@;$ConfigStoragePassword;g$\r$\n"
  ${EndIf}
  ${If} "$ConfigDirectorName" != ""
    FileWrite $R1 "s;@director_name@;$ConfigDirectorName;g$\r$\n"
  ${EndIf}
  ${If} "$ConfigDirectorPort" != ""
    FileWrite $R1 "s;@director_port@;$ConfigDirectorPort;g$\r$\n"
  ${EndIf}
  ${If} "$ConfigDirectorMaxJobs" != ""
    FileWrite $R1 "s;@director_maxjobs@;$ConfigDirectorMaxJobs;g$\r$\n"
  ${EndIf}
  ${If} "$ConfigDirectorPassword" != ""
    FileWrite $R1 "s;@director_password@;$ConfigDirectorPassword;g$\r$\n"
  ${EndIf}
  ${If} "$ConfigDirectorMailServer" != ""
    FileWrite $R1 "s;@smtp_host@;$ConfigDirectorMailServer;g$\r$\n"
  ${EndIf}
  ${If} "$ConfigDirectorMailAddress" != ""
    FileWrite $R1 "s;@job_email@;$ConfigDirectorMailAddress;g$\r$\n"
  ${EndIf}
  ${If} "$ConfigMonitorName" != ""
    FileWrite $R1 "s;@monitor_name@;$ConfigMonitorName;g$\r$\n"
  ${EndIf}
  ${If} "$ConfigMonitorPassword" != ""
    FileWrite $R1 "s;@monitor_password@;$ConfigMonitorPassword;g$\r$\n"
  ${EndIf}

  ${If} $ConfigDirectorDB = 1
    ${If} $MySQLPath != ""
      ${StrRep} $R2 "$MySQLPath\bin" "\" "\\"
      FileWrite $R1 "s;@SQL_BINDIR@;$R2;g$\r$\n"
    ${EndIf}
  ${ElseIf} $ConfigDirectorDB = 2
    ${If} $PostgreSQLPath != ""
      ${StrRep} $R2 "$PostgreSQLPath\bin" "\" "\\"
      FileWrite $R1 "s;@SQL_BINDIR@;$R2;g$\r$\n"
    ${EndIf}
  ${EndIf}

  FileClose $R1

  ${If} $InstallType = ${MigrateInstall}
    FileOpen $R1 $PLUGINSDIR\migrate.sed w
    ${StrRep} $R2 "$APPDATA\Bacula\Work" "\" "\\\\"
    FileWrite $R1 's;\(Working *Directory *= *\)[^ ][^ ]*.*$$;\1"$R2";$\r$\n'
    FileWrite $R1 's;\(Pid *Directory *= *\)[^ ][^ ]*.*$$;\1"$R2";$\r$\n'
    FileClose $R1
  ${EndIf}

  ${If} ${FileExists} "$OldInstallDir\bin\bacula-fd.exe"
    nsExec::ExecToLog '"$OldInstallDir\bin\bacula-fd.exe" /kill'     ; Shutdown any bacula that could be running
    Sleep 3000
    nsExec::ExecToLog '"$OldInstallDir\bin\bacula-fd.exe" /remove'   ; Remove existing service
  ${EndIf}

  ${If} ${FileExists} "$OldInstallDir\bin\bacula-sd.exe"
    nsExec::ExecToLog '"$OldInstallDir\bin\bacula-sd.exe" /kill'     ; Shutdown any bacula that could be running
    Sleep 3000
    nsExec::ExecToLog '"$OldInstallDir\bin\bacula-sd.exe" /remove'   ; Remove existing service
  ${EndIf}

  ${If} ${FileExists} "$OldInstallDir\bin\bacula-dir.exe"
    nsExec::ExecToLog '"$OldInstallDir\bin\bacula-dir.exe" /kill'     ; Shutdown any bacula that could be running
    Sleep 3000
    nsExec::ExecToLog '"$OldInstallDir\bin\bacula-dir.exe" /remove'   ; Remove existing service
  ${EndIf}

SectionEnd

SectionGroup "Client" SecGroupClient

Section "File Service" SecFileDaemon
  SectionIn 1 2 3

  SetOutPath "$INSTDIR\bin\fdplugins"
  File "${SRC_DIR}\exchange-fd.dll"

  SetOutPath "$INSTDIR\bin"

  File "${SRC_DIR}\bacula-fd.exe"

  ${If} $InstallType = ${MigrateInstall}
  ${AndIf} ${FileExists} "$OldInstallDir\bin\bacula-fd.conf"
    CopyFiles "$OldInstallDir\bin\bacula-fd.conf" "$APPDATA\Bacula"
    nsExec::ExecToLog '$PLUGINSDIR\sed.exe -f "$PLUGINSDIR\migrate.sed" -i.bak "$APPDATA\Bacula\bacula-fd.conf"'
  ${Else}
    File "/oname=$PLUGINSDIR\bacula-fd.conf" "bacula-fd.conf.in"

    StrCpy $0 "$APPDATA\Bacula"
    StrCpy $1 bacula-fd.conf
    Call ConfigEditAndCopy
  ${EndIf}

  StrCpy $0 bacula-fd
  StrCpy $1 "File Service"
  StrCpy $2 $ConfigClientInstallService
  StrCpy $3 $ConfigClientStartService

  Call InstallDaemon

  CreateShortCut "$SMPROGRAMS\Bacula\Configuration\Edit Client Configuration.lnk" "write.exe" '"$APPDATA\Bacula\bacula-fd.conf"'
SectionEnd

SectionGroupEnd

SectionGroup "Server" SecGroupServer

Section "Storage Service" SecStorageDaemon
  SectionIn 2 3

  SetOutPath "$INSTDIR\bin"

  File "${SRC_DIR}\loaderinfo.exe"
  File "${SRC_DIR}\mt.exe"
  File "${SRC_DIR}\mtx.exe"
  File "${SRC_DIR}\scsitape.exe"
  File "${SRC_DIR}\tapeinfo.exe"
  File "${SRC_DIR}\bacula-sd.exe"
  File "${SRC_DIR}\bcopy.exe"
  File "${SRC_DIR}\bextract.exe"
  File "${SRC_DIR}\bls.exe"
  File "${SRC_DIR}\bscan.exe"
  File "${SRC_DIR}\btape.exe"
  File "${SRC_DIR}\scsilist.exe"
  File "${SRC_DIR}\mkisofs.exe"
  File "${SRC_DIR}\growisofs.exe"
  File "${SRC_DIR}\dvd-ram-control.exe"
  File "${SRC_DIR}\dvd+rw-booktype.exe"
  File "${SRC_DIR}\dvd+rw-format.exe"
  File "${SRC_DIR}\dvd+rw-mediainfo.exe"

  File "/oname=$PLUGINSDIR\mtx-changer.cmd" "${SRC_DIR}\mtx-changer.cmd"

  StrCpy $0 "$INSTDIR\bin"
  StrCpy $1 mtx-changer.cmd
  Call ConfigEditAndCopy

  File "/oname=$PLUGINSDIR\disk-changer.cmd" "${SRC_DIR}\disk-changer.cmd"

  StrCpy $0 "$INSTDIR\bin"
  StrCpy $1 disk-changer.cmd
  Call ConfigEditAndCopy

  File "/oname=$PLUGINSDIR\dvd-handler.cmd" "${SRC_DIR}\dvd-handler.cmd"

  StrCpy $0 "$INSTDIR\bin"
  StrCpy $1 dvd-handler.cmd
  Call ConfigEditAndCopy

  File "/oname=$PLUGINSDIR\bacula-sd.conf" "bacula-sd.conf.in"

  StrCpy $0 "$APPDATA\Bacula"
  StrCpy $1 bacula-sd.conf
  Call ConfigEditAndCopy

  StrCpy $0 bacula-sd
  StrCpy $1 "Storage Service"
  StrCpy $2 $ConfigStorageInstallService
  StrCpy $3 $ConfigStorageStartService
  Call InstallDaemon

  CreateShortCut "$SMPROGRAMS\Bacula\Configuration\List Devices.lnk" "$INSTDIR\bin\scsilist.exe" "/pause"
  CreateShortCut "$SMPROGRAMS\Bacula\Configuration\Edit Storage Configuration.lnk" "write.exe" '"$APPDATA\Bacula\bacula-sd.conf"'
SectionEnd

Section "Director Service" SecDirectorDaemon
  SectionIn 2 3

  SetOutPath "$INSTDIR\bin"

  ${If} $ConfigDirectorDB = 1
    File /oname=bacula_cats.dll "${SRC_DIR}\cats_mysql.dll"
    File "${SRC_DIR}\libmysql.dll"
    File /oname=$PLUGINSDIR\create_database.cmd ${SRC_DIR}\create_mysql_database.cmd
    File /oname=$PLUGINSDIR\drop_database.cmd ${SRC_DIR}\drop_mysql_database.cmd
    File /oname=$PLUGINSDIR\make_tables.cmd ${SRC_DIR}\make_mysql_tables.cmd
    File ${SRC_DIR}\make_mysql_tables.sql
    File /oname=$PLUGINSDIR\drop_tables.cmd ${SRC_DIR}\drop_mysql_tables.cmd
    File ${SRC_DIR}\drop_mysql_tables.sql
    File /oname=$PLUGINSDIR\grant_privileges.cmd ${SRC_DIR}\grant_mysql_privileges.cmd
    File ${SRC_DIR}\grant_mysql_privileges.sql
    File /oname=$PLUGINSDIR\make_catalog_backup.cmd ${SRC_DIR}\make_mysql_catalog_backup.cmd
  ${ElseIf} $ConfigDirectorDB = 2
    File /oname=bacula_cats.dll "${SRC_DIR}\cats_postgresql.dll"
    File "${SRC_DIR}\libpq.dll"
!if "${BUILD_TOOLS}" == "VC8"
    File "${SRC_DIR}\comerr32.dll"
    File "${SRC_DIR}\libintl-2.dll"
    File "${SRC_DIR}\libiconv-2.dll"
    File "${SRC_DIR}\krb5_32.dll"
!endif
!If "${BUILD_TOOLS}" == "VC8_DEBUG"
    File "${SRC_DIR}\comerr32.dll"
    File "${SRC_DIR}\libintl-2.dll"
    File "${SRC_DIR}\libiconv-2.dll"
    File "${SRC_DIR}\krb5_32.dll"
!endif
    File /oname=$PLUGINSDIR\create_database.cmd ${SRC_DIR}\create_postgresql_database.cmd
    File /oname=$PLUGINSDIR\drop_database.cmd ${SRC_DIR}\drop_postgresql_database.cmd
    File /oname=$PLUGINSDIR\make_tables.cmd ${SRC_DIR}\make_postgresql_tables.cmd
    File ${SRC_DIR}\create_postgresql_database.sql
    File ${SRC_DIR}\make_postgresql_tables.sql
    File /oname=$PLUGINSDIR\drop_tables.cmd ${SRC_DIR}\drop_postgresql_tables.cmd
    File ${SRC_DIR}\drop_postgresql_tables.sql
    File /oname=$PLUGINSDIR\grant_privileges.cmd ${SRC_DIR}\grant_postgresql_privileges.cmd
    File ${SRC_DIR}\grant_postgresql_privileges.sql
    File /oname=$PLUGINSDIR\make_catalog_backup.cmd ${SRC_DIR}\make_postgresql_catalog_backup.cmd
  ${ElseIf} $ConfigDirectorDB = 3
    File "${SRC_DIR}\sqlite3.exe"
!if "${BUILD_TOOLS}" == "VC8"
    File "${SRC_DIR}\sqlite3.exe.manifest"
!endif
!If "${BUILD_TOOLS}" == "VC8_DEBUG"
    File "${SRC_DIR}\sqlite3.exe.manifest"
!endif
    File /oname=bacula_cats.dll "${SRC_DIR}\cats_sqlite3.dll"
    File /oname=$PLUGINSDIR\create_database.cmd ${SRC_DIR}\create_sqlite3_database.cmd
    File /oname=$PLUGINSDIR\drop_database.cmd ${SRC_DIR}\drop_sqlite3_database.cmd
    File /oname=$PLUGINSDIR\make_tables.cmd ${SRC_DIR}\make_sqlite3_tables.cmd
    File ${SRC_DIR}\make_sqlite3_tables.sql
    File /oname=$PLUGINSDIR\drop_tables.cmd ${SRC_DIR}\drop_sqlite3_tables.cmd
    File /oname=$PLUGINSDIR\grant_privileges.cmd ${SRC_DIR}\grant_sqlite3_privileges.cmd
    File /oname=$PLUGINSDIR\make_catalog_backup.cmd ${SRC_DIR}\make_sqlite3_catalog_backup.cmd
  ${EndIf}

  File "${SRC_DIR}\bacula-dir.exe"
  File "${SRC_DIR}\dbcheck.exe"

  File "/oname=$PLUGINSDIR\delete_catalog_backup.cmd" "${SRC_DIR}\delete_catalog_backup.cmd"

  StrCpy $0 "$INSTDIR\bin"
  
  StrCpy $1 create_database.cmd
  Call ConfigEditAndCopy

  StrCpy $1 drop_database.cmd
  Call ConfigEditAndCopy

  StrCpy $1 make_tables.cmd
  Call ConfigEditAndCopy

  StrCpy $1 drop_tables.cmd
  Call ConfigEditAndCopy

  StrCpy $1 grant_privileges.cmd
  Call ConfigEditAndCopy

  StrCpy $1 make_catalog_backup.cmd
  Call ConfigEditAndCopy

  StrCpy $1 delete_catalog_backup.cmd
  Call ConfigEditAndCopy

  File "${SRC_DIR}\query.sql"

  File "/oname=$PLUGINSDIR\bacula-dir.conf" "bacula-dir.conf.in"

  StrCpy $0 "$APPDATA\Bacula"
  StrCpy $1 bacula-dir.conf
  Call ConfigEditAndCopy

  StrCpy $0 bacula-dir
  StrCpy $1 "Director Service"
  StrCpy $2 $ConfigDirectorInstallService
  StrCpy $3 $ConfigDirectorStartService
  Call InstallDaemon

  CreateShortCut "$SMPROGRAMS\Bacula\Configuration\Edit Director Configuration.lnk" "write.exe" '"$APPDATA\Bacula\bacula-dir.conf"'
SectionEnd

SectionGroupEnd

SectionGroup "Consoles" SecGroupConsoles

Section "Command Console" SecConsole
  SectionIn 1 2 3

  SetOutPath "$INSTDIR\bin"

  File "${SRC_DIR}\bconsole.exe"
  Call InstallCommonFiles

  ${If} $InstallType = ${MigrateInstall}
  ${AndIf} ${FileExists} "$OldInstallDir\bin\bconsole.conf"
    CopyFiles "$OldInstallDir\bin\bconsole.conf" "$APPDATA\Bacula"
  ${Else}
    File "/oname=$PLUGINSDIR\bconsole.conf" "bconsole.conf.in"
    StrCpy $0 "$APPDATA\Bacula"
    StrCpy $1 bconsole.conf
    Call ConfigEditAndCopy
  ${EndIf}

  CreateShortCut "$SMPROGRAMS\Bacula\bconsole.lnk" "$INSTDIR\bin\bconsole.exe" '-c "$APPDATA\Bacula\bconsole.conf"' "$INSTDIR\bin\bconsole.exe" 0
  CreateShortCut "$SMPROGRAMS\Bacula\Configuration\Edit Command Console Configuration.lnk" "write.exe" '"$APPDATA\Bacula\bconsole.conf"'

SectionEnd

Section "Bat Console" SecBatConsole
  SectionIn 1 2 3
  
  SetOutPath "$INSTDIR\bin"

  Call InstallCommonFiles
  File "${SRC_DIR}\QtCore4.dll"
  File "${SRC_DIR}\QtGui4.dll"

  File "${SRC_DIR}\bat.exe"

  ${If} $InstallType = ${MigrateInstall}
  ${AndIf} ${FileExists} "$OldInstallDir\bin\bat.conf"
    CopyFiles "$OldInstallDir\bin\bat.conf" "$APPDATA\Bacula"
  ${Else}
    File "/oname=$PLUGINSDIR\bat.conf" "bat.conf.in"
    StrCpy $0 "$APPDATA\Bacula"
    StrCpy $1 bat.conf
    Call ConfigEditAndCopy
  ${EndIf}

  ; Create Start Menu entry
  CreateShortCut "$SMPROGRAMS\Bacula\Bat.lnk" "$INSTDIR\bin\bat.exe" '-c "$APPDATA\Bacula\bat.conf"' "$INSTDIR\bin\bat.exe" 0
  CreateShortCut "$SMPROGRAMS\Bacula\Configuration\Edit Bat Configuration.lnk" "write.exe" '"$APPDATA\Bacula\bat.conf"'
SectionEnd


Section "Graphical Console" SecWxConsole
  SectionIn 1 2 3
  
  SetOutPath "$INSTDIR\bin"

  Call InstallCommonFiles
!if "${BUILD_TOOLS}" == "VC8"
  File "${SRC_DIR}\wxbase28_vc_bacula.dll"
  File "${SRC_DIR}\wxmsw28_core_vc_bacula.dll"
!endif
!If "${BUILD_TOOLS}" == "VC8_DEBUG"
  File "${SRC_DIR}\wxbase28_vc_bacula.dll"
  File "${SRC_DIR}\wxmsw28_core_vc_bacula.dll"
!endif
!if "${BUILD_TOOLS}" == "MinGW"
  File "${SRC_DIR}\wxbase28_gcc_bacula.dll"
  File "${SRC_DIR}\wxmsw28_core_gcc_bacula.dll"
!endif

  File "${SRC_DIR}\bwx-console.exe"

  ${If} $InstallType = ${MigrateInstall}
  ${AndIf} ${FileExists} "$OldInstallDir\bin\bwx-console.conf"
    CopyFiles "$OldInstallDir\bin\bwx-console.conf" "$APPDATA\Bacula"
  ${Else}
    File "/oname=$PLUGINSDIR\bwx-console.conf" "bwx-console.conf.in"
    StrCpy $0 "$APPDATA\Bacula"
    StrCpy $1 bwx-console.conf
    Call ConfigEditAndCopy
  ${EndIf}

  ; Create Start Menu entry
  CreateShortCut "$SMPROGRAMS\Bacula\bwx-console.lnk" "$INSTDIR\bin\bwx-console.exe" '-c "$APPDATA\Bacula\bwx-console.conf"' "$INSTDIR\bin\bwx-console.exe" 0
  CreateShortCut "$SMPROGRAMS\Bacula\Configuration\Edit Graphical Console Configuration.lnk" "write.exe" '"$APPDATA\Bacula\bwx-console.conf"'
SectionEnd

SectionGroupEnd

SectionGroup "Documentation" SecGroupDocumentation

Section "Documentation (Acrobat Format)" SecDocPdf
  SectionIn 1 2 3

  SetOutPath "$INSTDIR\doc"
  CreateDirectory "$INSTDIR\doc"

  File "${SRC_DIR}\docs\manuals\en\console\console.pdf"
  File "${SRC_DIR}\docs\manuals\en\misc\misc.pdf"
  File "${SRC_DIR}\docs\manuals\en\main\main.pdf"
  File "${SRC_DIR}\docs\manuals\en\utility\utility.pdf"
  File "${SRC_DIR}\docs\manuals\en\problems\problems.pdf"
  CreateShortCut "$SMPROGRAMS\Bacula\Documentation\Console.lnk" '"$INSTDIR\doc\console.pdf"'
  CreateShortCut "$SMPROGRAMS\Bacula\Documentation\Main.lnk" '"$INSTDIR\doc\main.pdf"'
  CreateShortCut "$SMPROGRAMS\Bacula\Documentation\Misc.lnk" '"$INSTDIR\doc\misc.pdf"'
  CreateShortCut "$SMPROGRAMS\Bacula\Documentation\Utility.lnk" '"$INSTDIR\doc\utility.pdf"'
  CreateShortCut "$SMPROGRAMS\Bacula\Documentation\Problems.lnk" '"$INSTDIR\doc\problems.pdf"'
SectionEnd

Section "Documentation (HTML Format)" SecDocHtml
  SectionIn 3

  SetOutPath "$INSTDIR\doc"
  CreateDirectory "$INSTDIR\doc"

; File "${SRC_DIR}\manual\bacula\*.html"
; File "${SRC_DIR}\manual\bacula\*.png"
; File "${SRC_DIR}\manual\bacula\*.css"
; CreateShortCut "$SMPROGRAMS\Bacula\Documentation\Manual (HTML).lnk" '"$INSTDIR\doc\index.html"'
SectionEnd

SectionGroupEnd

Section "-Finish"
  Push $R0

  ${If} $OsIsNT = 1
    nsExec::ExecToLog 'cmd.exe /C echo Y|cacls "$INSTDIR" /T /G SYSTEM:F Administrators:F'
    nsExec::ExecToLog 'cmd.exe /C echo Y|cacls "$APPDATA\Bacula" /T /G SYSTEM:F Administrators:F'
  ${EndIf}

  ; Write the uninstall keys for Windows & create Start Menu entry
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Bacula" "DisplayName" "Bacula"
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Bacula" "InstallLocation" "$INSTDIR"
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Bacula" "DisplayVersion" "${VERSION}"
  ${StrTok} $R0 "${VERSION}" "." 0 0
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Bacula" "VersionMajor" $R0
  ${StrTok} $R0 "${VERSION}" "." 1 0
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Bacula" "VersionMinor" $R0
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Bacula" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Bacula" "NoRepair" 1
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Bacula" "URLUpdateInfo" "http://sourceforge.net/project/showfiles.php?group_id=50727"
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Bacula" "URLInfoAbout" "http://www.bacula.org"
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Bacula" "HelpLink" "http://www.bacula.org/?page=support"
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Bacula" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  CreateShortCut "$SMPROGRAMS\Bacula\Uninstall Bacula.lnk" "$INSTDIR\Uninstall.exe" "" "$INSTDIR\Uninstall.exe" 0
  Pop $R0
SectionEnd

; Extra Page descriptions

LangString DESC_SecFileDaemon ${LANG_ENGLISH} "Install Bacula File Daemon on this system."
LangString DESC_SecStorageDaemon ${LANG_ENGLISH} "Install Bacula Storage Daemon on this system."
LangString DESC_SecDirectorDaemon ${LANG_ENGLISH} "Install Bacula Director Daemon on this system."
LangString DESC_SecConsole ${LANG_ENGLISH} "Install command console program on this system."
LangString DESC_SecBatConsole ${LANG_ENGLISH} "Install Bat graphical console program on this system."
LangString DESC_SecWxConsole ${LANG_ENGLISH} "Install wxWidgets graphical console program on this system."
LangString DESC_SecDocPdf ${LANG_ENGLISH} "Install documentation in Acrobat format on this system."
LangString DESC_SecDocHtml ${LANG_ENGLISH} "Install documentation in HTML format on this system."

LangString TITLE_ConfigPage1 ${LANG_ENGLISH} "Configuration"
LangString SUBTITLE_ConfigPage1 ${LANG_ENGLISH} "Set installation configuration."

LangString TITLE_ConfigPage2 ${LANG_ENGLISH} "Configuration (continued)"
LangString SUBTITLE_ConfigPage2 ${LANG_ENGLISH} "Set installation configuration."

LangString TITLE_InstallType ${LANG_ENGLISH} "Installation Type"
LangString SUBTITLE_InstallType ${LANG_ENGLISH} "Choose installation type."

LangString TITLE_WriteTemplates ${LANG_ENGLISH} "Create Templates"
LangString SUBTITLE_WriteTemplates ${LANG_ENGLISH} "Create resource templates for inclusion in the Director's configuration file."

!InsertMacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !InsertMacro MUI_DESCRIPTION_TEXT ${SecFileDaemon} $(DESC_SecFileDaemon)
  !InsertMacro MUI_DESCRIPTION_TEXT ${SecStorageDaemon} $(DESC_SecStorageDaemon)
  !InsertMacro MUI_DESCRIPTION_TEXT ${SecDirectorDaemon} $(DESC_SecDirectorDaemon)
  !InsertMacro MUI_DESCRIPTION_TEXT ${SecConsole} $(DESC_SecConsole)
  !InsertMacro MUI_DESCRIPTION_TEXT ${SecBatConsole} $(DESC_SecBatConsole)
  !InsertMacro MUI_DESCRIPTION_TEXT ${SecWxConsole} $(DESC_SecWxConsole)
  !InsertMacro MUI_DESCRIPTION_TEXT ${SecDocPdf} $(DESC_SecDocPdf)
  !InsertMacro MUI_DESCRIPTION_TEXT ${SecDocHtml} $(DESC_SecDocHtml)
!InsertMacro MUI_FUNCTION_DESCRIPTION_END

; Uninstall section

UninstallText "This will uninstall Bacula. Hit next to continue."

Section "Uninstall"
  ; Shutdown any baculum that could be running
  nsExec::ExecToLog '"$INSTDIR\bin\bacula-fd.exe" /kill'
  nsExec::ExecToLog '"$INSTDIR\bin\bacula-sd.exe" /kill'
  nsExec::ExecToLog '"$INSTDIR\bin\bacula-dir.exe" /kill'
  Sleep 3000

  ReadRegDWORD $R0 HKLM "Software\Bacula" "Service_Bacula-fd"
  ${If} $R0 = 1
    ; Remove bacula service
    nsExec::ExecToLog '"$INSTDIR\bin\bacula-fd.exe" /remove'
    nsExec::ExecToLog '"$INSTDIR\bin\exchange-fd.dll" /remove'
  ${EndIf}
  
  ReadRegDWORD $R0 HKLM "Software\Bacula" "Service_Bacula-sd"
  ${If} $R0 = 1
    ; Remove bacula service
    nsExec::ExecToLog '"$INSTDIR\bin\bacula-sd.exe" /remove'
  ${EndIf}
  
  ReadRegDWORD $R0 HKLM "Software\Bacula" "Service_Bacula-dir"
  ${If} $R0 = 1
    ; Remove bacula service
    nsExec::ExecToLog '"$INSTDIR\bin\bacula-dir.exe" /remove'
  ${EndIf}
  
  ; remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Bacula"
  DeleteRegKey HKLM "Software\Bacula"

  ; remove start menu items
  SetShellVarContext all
  Delete /REBOOTOK "$SMPROGRAMS\Bacula\*"
  RMDir "$SMPROGRAMS\Bacula"

  ; remove files and uninstaller (preserving config for now)
  Delete /REBOOTOK "$INSTDIR\bin\*"
  Delete /REBOOTOK "$INSTDIR\doc\*"
  Delete /REBOOTOK "$INSTDIR\*"

  ; Check for existing installation
  MessageBox MB_YESNO|MB_ICONQUESTION \
  "Would you like to delete the current configuration files and the working state file?" IDNO NoDel
    Delete /REBOOTOK "$APPDATA\Bacula\*"
    Delete /REBOOTOK "$APPDATA\Bacula\Work\*"
    Delete /REBOOTOK "$APPDATA\Bacula\Spool\*"
    Delete /REBOOTOK "$PLUGINSDIR\bacula-*.conf"
    Delete /REBOOTOK "$PLUGINSDIR\*console.conf"
    Delete /REBOOTOK "$PLUGINSDIR\*conf.in"
    Delete /REBOOTOK "$PLUGINSDIR\openssl.exe"
    Delete /REBOOTOK "$PLUGINSDIR\libeay32.dll"
    Delete /REBOOTOK "$PLUGINSDIR\ssleay32.dll"
    Delete /REBOOTOK "$PLUGINSDIR\sed.exe"    
    Delete /REBOOTOK "$PLUGINSDIR\pw.txt"     
    Delete /REBOOTOK "$PLUGINSDIR\*.sed" 
    Delete /REBOOTOK "$PLUGINSDIR\*.cmd"    
    Delete /REBOOTOK "$PLUGINSDIR\*.sql"    
    RMDir "$APPDATA\Bacula\Work"
    RMDir "$APPDATA\Bacula\Spool"
    RMDir "$APPDATA\Bacula"
NoDel:

  ; remove directories used
  RMDir "$INSTDIR\bin"
  RMDir "$INSTDIR\doc"
  RMDir "$INSTDIR"
SectionEnd

;
; $0 - Service Name (ie Bacula-FD)
; $1 - Service Description (ie Bacula File Daemon)
; $2 - Install as Service
; $3 - Start Service now
;
Function InstallDaemon
  Call InstallCommonFiles

  WriteRegDWORD HKLM "Software\Bacula" "Service_$0" $2
  
  ${If} $2 = 1
    nsExec::ExecToLog '"$INSTDIR\bin\$0.exe" /install -c "$APPDATA\Bacula\$0.conf"'

    ${If} $OsIsNT <> 1
      File "Start.bat"
      File "Stop.bat"
    ${EndIf}

    ; Start the service?

    ${If} $3 = 1  
      ${If} $OsIsNT = 1
        nsExec::ExecToLog 'net start $0'
      ${Else}
        Exec '"$INSTDIR\bin\$0.exe" -c "$APPDATA\Bacula\$0.conf"'
      ${EndIf}
    ${EndIf}
  ${Else}
    CreateShortCut "$SMPROGRAMS\Bacula\Start $1.lnk" "$INSTDIR\bin\$0.exe" '-c "$APPDATA\Bacula\$0.conf"' "$INSTDIR\bin\$0.exe" 0
  ${EndIf}
FunctionEnd

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

!define NameUserPrincipal 8

Function GetUserName
  Push $R0
  Push $R1
  Push $R2

  ${If} $OsIsNT = 1
    System::Call "secur32::GetUserNameExA(i ${NameUserPrincipal}, t .R0, *i ${NSIS_MAX_STRLEN} R1) i.R2 ?e"
    ${If} $R2 = 0
      Pop $R2
      DetailPrint "GetUserNameExA failed - LastError = $R2"
      Pop $R0
      StrCpy $R0 ""
    ${Else}
      Pop $R2
    ${EndIf}
  ${Else}
      StrCpy $R0 ""
  ${EndIf}

  ${If} $R0 == ""
    System::Call "advapi32::GetUserNameA(t .R0, *i ${NSIS_MAX_STRLEN} R1) i.R2 ?e"
    ${If} $R2 = 0
      Pop $R2
      DetailPrint "GetUserNameA failed - LastError = $R2"
      StrCpy $R0 ""
    ${Else}
      Pop $R2
    ${EndIf}
  ${EndIf}

  Pop $R2
  Pop $R1
  Exch $R0
FunctionEnd

Function ConfigEditAndCopy
  Push $R1

  ${If} ${FileExists} "$0\$1"
    StrCpy $R1 ".new"
  ${Else}
    StrCpy $R1 ""
  ${EndIf}

  nsExec::ExecToLog '$PLUGINSDIR\sed.exe -f "$PLUGINSDIR\config.sed" -i.bak "$PLUGINSDIR\$1"'
  CopyFiles "$PLUGINSDIR\$1" "$0\$1$R1"

  Pop $R1
FunctionEnd

Function FindDatabaseApps
  Push $R1

  ReadRegStr $0 HKLM "Software\MySQL AB\MySQL Server 5.0" "Location"

  ${If} $0 != ""
    Call RemoveTrailingSlash
    StrCpy $MySQLPath $0
    ReadRegStr $0 HKLM "Software\MySQL AB\MySQL Server 5.0" "Version"
    StrCpy $MySQLVersion $0
  ${EndIf}

  EnumRegKey $R1 HKLM "Software\PostgreSQL\Installations" 0
  ${If} $R1 != ""
    ReadRegStr $0 HKLM "Software\PostgreSQL\Installations\$R1" "Base Directory"
    Call RemoveTrailingSlash
    StrCpy $PostgreSQLPath $0
    ReadRegStr $0 HKLM "Software\PostgreSQL\Installations\$R1" "Version"
    StrCpy $PostgreSQLVersion $0
  ${EndIf}

  Pop $R1
FunctionEnd

Function RemoveTrailingSlash
  Push $R1
  StrCpy $R1 $0 "" -1
  ${If} $R1 == "\"
    StrCpy $0 $0 -1
  ${EndIf}
  Pop $R1
FunctionEnd

Function IsDirectorSelected
  Push $R0
  SectionGetFlags ${SecDirectorDaemon} $R0
  IntOp $R0 $R0 & ${SF_SELECTED}
  Exch $R0
FunctionEnd

Function GetSelectedComponents
  Push $R0
  StrCpy $R0 0
  ${If} ${SectionIsSelected} ${SecFileDaemon}
    IntOp $R0 $R0 | ${ComponentFile}
  ${EndIf}
  ${If} ${SectionIsSelected} ${SecStorageDaemon}
    IntOp $R0 $R0 | ${ComponentStorage}
  ${EndIf}
  ${If} ${SectionIsSelected} ${SecDirectorDaemon}
    IntOp $R0 $R0 | ${ComponentDirector}
  ${EndIf}
  ${If} ${SectionIsSelected} ${SecConsole}
    IntOp $R0 $R0 | ${ComponentTextConsole}
  ${EndIf}
  ${If} ${SectionIsSelected} ${SecBatConsole}
    IntOp $R0 $R0 | ${ComponentBatConsole}
  ${EndIf}
  ${If} ${SectionIsSelected} ${SecWxConsole}
    IntOp $R0 $R0 | ${ComponentGUIConsole}
  ${EndIf}
  ${If} ${SectionIsSelected} ${SecDocPdf}
    IntOp $R0 $R0 | ${ComponentPDFDocs}
  ${EndIf}
  ${If} ${SectionIsSelected} ${SecDocHtml}
    IntOp $R0 $R0 | ${ComponentHTMLDocs}
  ${EndIf}
  Exch $R0
FunctionEnd

Function PageComponentsShow
  ${If} $OsIsNT <> 1
    Call DisableServerSections
  ${EndIf}

  Call SelectPreviousComponents
  Call UpdateComponentUI
FunctionEnd

Function PageDirectoryPre
  ${If} $AutomaticInstall = 1
  ${OrIf} $InstallType = ${UpgradeInstall}
    Abort
  ${EndIf}
FunctionEnd

Function LeaveInstallPage
  Push "$INSTDIR\install.log"
  Call DumpLog
FunctionEnd

Function EnterWriteTemplates
  Push $R0
  Push $R1

  Call GetSelectedComponents
  Pop $R0

  IntOp $R0 $R0 & ${ComponentDirector}
  IntOp $R1 $NewComponents & ${ComponentsFileAndStorage}

  ${If} $R0 <> 0
  ${OrIf} $R1 = 0
    Pop $R1
    Pop $R0
    Abort
  ${EndIf}

  IntOp $R0 $NewComponents & ${ComponentFile}
  ${If} $R0 = 0
    WriteINIStr "$PLUGINSDIR\WriteTemplates.ini" "Field 2" State 0
    WriteINIStr "$PLUGINSDIR\WriteTemplates.ini" "Field 2" Flags DISABLED
    DeleteINIStr "$PLUGINSDIR\WriteTemplates.ini" "Field 3" State
    WriteINIStr "$PLUGINSDIR\WriteTemplates.ini" "Field 3" Flags REQ_SAVE|FILE_EXPLORER|WARN_IF_EXIST|DISABLED
  ${Else}
    WriteINIStr "$PLUGINSDIR\WriteTemplates.ini" "Field 2" State 1
    DeleteINIStr "$PLUGINSDIR\WriteTemplates.ini" "Field 2" Flags
    WriteINIStr "$PLUGINSDIR\WriteTemplates.ini" "Field 3" State "C:\$ConfigClientName.conf"
    WriteINIStr "$PLUGINSDIR\WriteTemplates.ini" "Field 5" Flags REQ_SAVE|FILE_EXPLORER|WARN_IF_EXIST
  ${EndIf}

  IntOp $R0 $NewComponents & ${ComponentStorage}
  ${If} $R0 = 0
    WriteINIStr "$PLUGINSDIR\WriteTemplates.ini" "Field 4" State 0
    WriteINIStr "$PLUGINSDIR\WriteTemplates.ini" "Field 4" Flags DISABLED
    DeleteINIStr "$PLUGINSDIR\WriteTemplates.ini" "Field 5" State
    WriteINIStr "$PLUGINSDIR\WriteTemplates.ini" "Field 5" Flags REQ_SAVE|FILE_EXPLORER|WARN_IF_EXIST|DISABLED
  ${Else}
    WriteINIStr "$PLUGINSDIR\WriteTemplates.ini" "Field 4" State 1
    DeleteINIStr "$PLUGINSDIR\WriteTemplates.ini" "Field 4" Flags
    WriteINIStr "$PLUGINSDIR\WriteTemplates.ini" "Field 5" State "C:\$ConfigStorageName.conf"
    WriteINIStr "$PLUGINSDIR\WriteTemplates.ini" "Field 5" Flags REQ_SAVE|FILE_EXPLORER|WARN_IF_EXIST
  ${EndIf}

  !InsertMacro MUI_HEADER_TEXT "$(TITLE_WriteTemplates)" "$(SUBTITLE_WriteTemplates)"
  !InsertMacro MUI_INSTALLOPTIONS_DISPLAY "WriteTemplates.ini"

  !InsertMacro MUI_INSTALLOPTIONS_READ $R0 "WriteTemplates.ini" "Field 2" State
  ${If} $R0 <> 0
    File "/oname=$PLUGINSDIR\client.conf.in" "client.conf.in"

    nsExec::ExecToLog '$PLUGINSDIR\sed.exe -f "$PLUGINSDIR\config.sed" -i.bak "$PLUGINSDIR\client.conf.in"'
    !InsertMacro MUI_INSTALLOPTIONS_READ $R0 "WriteTemplates.ini" "Field 3" State
    ${If} $R0 != ""
      CopyFiles "$PLUGINSDIR\client.conf.in" "$R0"
    ${EndIf}
  ${EndIf}

  !InsertMacro MUI_INSTALLOPTIONS_READ $R0 "WriteTemplates.ini" "Field 4" State
  ${If} $R0 <> 0
    File "/oname=$PLUGINSDIR\storage.conf.in" "storage.conf.in"

    nsExec::ExecToLog '$PLUGINSDIR\sed.exe -f "$PLUGINSDIR\config.sed" -i.bak "$PLUGINSDIR\storage.conf.in"'
    !InsertMacro MUI_INSTALLOPTIONS_READ $R0 "WriteTemplates.ini" "Field 5" State
    ${If} $R0 != ""
      CopyFiles "$PLUGINSDIR\storage.conf.in" "$R0"
    ${EndIf}
  ${EndIf}

  Pop $R1
  Pop $R0
FunctionEnd

Function SelectPreviousComponents
  ${If} $InstallType <> ${NewInstall}
    IntOp $R1 $PreviousComponents & ${ComponentFile}
    ${If} $R1 <> 0
      !InsertMacro SelectSection ${SecFileDaemon}
      !InsertMacro SetSectionFlag ${SecFileDaemon} ${SF_RO}
    ${Else}
      !InsertMacro UnselectSection ${SecFileDaemon}
      !InsertMacro ClearSectionFlag ${SecFileDaemon} ${SF_RO}
    ${EndIf}
    IntOp $R1 $PreviousComponents & ${ComponentStorage}
    ${If} $R1 <> 0
      !InsertMacro SelectSection ${SecStorageDaemon}
      !InsertMacro SetSectionFlag ${SecStorageDaemon} ${SF_RO}
    ${Else}
      !InsertMacro UnselectSection ${SecStorageDaemon}
      !InsertMacro ClearSectionFlag ${SecStorageDaemon} ${SF_RO}
    ${EndIf}
    IntOp $R1 $PreviousComponents & ${ComponentDirector}
    ${If} $R1 <> 0
      !InsertMacro SelectSection ${SecDirectorDaemon}
      !InsertMacro SetSectionFlag ${SecDirectorDaemon} ${SF_RO}
    ${Else}
      !InsertMacro UnselectSection ${SecDirectorDaemon}
      !InsertMacro ClearSectionFlag ${SecDirectorDaemon} ${SF_RO}
    ${EndIf}
    IntOp $R1 $PreviousComponents & ${ComponentTextConsole}
    ${If} $R1 <> 0
      !InsertMacro SelectSection ${SecConsole}
      !InsertMacro SetSectionFlag ${SecConsole} ${SF_RO}
    ${Else}
      !InsertMacro UnselectSection ${SecConsole}
      !InsertMacro ClearSectionFlag ${SecConsole} ${SF_RO}
    ${EndIf}
    IntOp $R1 $PreviousComponents & ${ComponentBatConsole}
    ${If} $R1 <> 0
      !InsertMacro SelectSection ${SecBatConsole}
      !InsertMacro SetSectionFlag ${SecBatConsole} ${SF_RO}
    ${Else}
      !InsertMacro UnselectSection ${SecBatConsole}
      !InsertMacro ClearSectionFlag ${SecBatConsole} ${SF_RO}
    ${EndIf}
    IntOp $R1 $PreviousComponents & ${ComponentGUIConsole}
    ${If} $R1 <> 0
      !InsertMacro SelectSection ${SecWxConsole}
      !InsertMacro SetSectionFlag ${SecWxConsole} ${SF_RO}
    ${Else}
      !InsertMacro UnselectSection ${SecWxConsole}
      !InsertMacro ClearSectionFlag ${SecWxConsole} ${SF_RO}
    ${EndIf}
    IntOp $R1 $PreviousComponents & ${ComponentPDFDocs}
    ${If} $R1 <> 0
      !InsertMacro SelectSection ${SecDocPdf}
      !InsertMacro SetSectionFlag ${SecDocPdf} ${SF_RO}
    ${Else}
      !InsertMacro UnselectSection ${SecDocPdf}
      !InsertMacro ClearSectionFlag ${SecDocPdf} ${SF_RO}
    ${EndIf}
    IntOp $R1 $PreviousComponents & ${ComponentHTMLDocs}
    ${If} $R1 <> 0
      !InsertMacro SelectSection ${SecDocHtml}
      !InsertMacro SetSectionFlag ${SecDocHtml} ${SF_RO}
    ${Else}
      !InsertMacro UnselectSection ${SecDocHtml}
      !InsertMacro ClearSectionFlag ${SecDocHtml} ${SF_RO}
    ${EndIf}
  ${EndIf}
FunctionEnd

Function DisableServerSections
  !InsertMacro UnselectSection ${SecGroupServer}
  !InsertMacro SetSectionFlag  ${SecGroupServer} ${SF_RO}
  !InsertMacro UnselectSection ${SecStorageDaemon}
  !InsertMacro SetSectionFlag  ${SecStorageDaemon} ${SF_RO}
  !InsertMacro UnselectSection ${SecDirectorDaemon}
  !InsertMacro SetSectionFlag  ${SecDirectorDaemon} ${SF_RO}
FunctionEnd

Function UpdateComponentUI
  Push $R0
  Push $R1

  Call GetSelectedComponents
  Pop $R0

  IntOp $R1 $R0 ^ $PreviousComponents
  IntOp $NewComponents $R0 & $R1

  ${If} $InstallType <> ${NewInstall}
    IntOp $R1 $NewComponents & ${ComponentFile}
    ${If} $R1 <> 0
      !InsertMacro SetSectionFlag ${SecFileDaemon} ${SF_BOLD}
    ${Else}
      !InsertMacro ClearSectionFlag ${SecFileDaemon} ${SF_BOLD}
    ${EndIf}
    IntOp $R1 $NewComponents & ${ComponentStorage}
    ${If} $R1 <> 0
      !InsertMacro SetSectionFlag ${SecStorageDaemon} ${SF_BOLD}
    ${Else}
      !InsertMacro ClearSectionFlag ${SecStorageDaemon} ${SF_BOLD}
    ${EndIf}
    IntOp $R1 $NewComponents & ${ComponentDirector}
    ${If} $R1 <> 0
      !InsertMacro SetSectionFlag ${SecDirectorDaemon} ${SF_BOLD}
    ${Else}
      !InsertMacro ClearSectionFlag ${SecDirectorDaemon} ${SF_BOLD}
    ${EndIf}
    IntOp $R1 $NewComponents & ${ComponentTextConsole}
    ${If} $R1 <> 0
      !InsertMacro SetSectionFlag ${SecConsole} ${SF_BOLD}
    ${Else}
      !InsertMacro ClearSectionFlag ${SecConsole} ${SF_BOLD}
    ${EndIf}
    IntOp $R1 $NewComponents & ${ComponentBatConsole}
    ${If} $R1 <> 0
      !InsertMacro SetSectionFlag ${SecBatConsole} ${SF_BOLD}
    ${Else}
      !InsertMacro ClearSectionFlag ${SecBatConsole} ${SF_BOLD}
    ${EndIf}
    IntOp $R1 $NewComponents & ${ComponentGUIConsole}
    ${If} $R1 <> 0
      !InsertMacro SetSectionFlag ${SecWxConsole} ${SF_BOLD}
    ${Else}
      !InsertMacro ClearSectionFlag ${SecWxConsole} ${SF_BOLD}
    ${EndIf}
    IntOp $R1 $NewComponents & ${ComponentPDFDocs}
    ${If} $R1 <> 0
      !InsertMacro SetSectionFlag ${SecDocPdf} ${SF_BOLD}
    ${Else}
      !InsertMacro ClearSectionFlag ${SecDocPdf} ${SF_BOLD}
    ${EndIf}
    IntOp $R1 $NewComponents & ${ComponentHTMLDocs}
    ${If} $R1 <> 0
      !InsertMacro SetSectionFlag ${SecDocHtml} ${SF_BOLD}
    ${Else}
      !InsertMacro ClearSectionFlag ${SecDocHtml} ${SF_BOLD}
    ${EndIf}
  ${EndIf}

  GetDlgItem $R0 $HWNDPARENT 1

  IntOp $R1 $NewComponents & ${ComponentsRequiringUserConfig}
  ${If} $R1 = 0
    SendMessage $R0 ${WM_SETTEXT} 0 "STR:Install"
  ${Else}
    SendMessage $R0 ${WM_SETTEXT} 0 "STR:&Next >"
  ${EndIf}

  Pop $R1
  Pop $R0
FunctionEnd

!include "InstallType.nsh"
!include "ConfigPage1.nsh"
!include "ConfigPage2.nsh"
!include "DumpLog.nsh"
