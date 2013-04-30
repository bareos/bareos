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
; Kern Sibbald October 2008
; Remove server installs
; Install into single bacula directory
;  (i.e. undo a large part of what Robert Nelson did)
;
; Eric Bollengier March 2009
; Updated to handle Win64 installation
;
; Kern Sibbald April 2009
; Correct some Win64 install problems
; It is mind boggling how many lines of this insane scripting language
;   have been written with absolutely no comments
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
OutFile "${OUT_DIR}\bacula-win${WINVER}-${VERSION}.exe"
SetCompressor lzma

InstallDir "C:\Program Files\Bacula"
InstallDirRegKey HKLM "Software\Bacula" "InstallLocation"

InstType "Client"
;InstType "Server"
;InstType "Full"

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
;     5 = wxWidgits Console
;     6 = Documentation (PDF)
;     7 = Documentation (HTML)
;    10 = Tray Monitor

!define ComponentFile                   1
!define ComponentStorage                2
!define ComponentDirector               4
!define ComponentTextConsole            8
!define ComponentBatConsole             16
!define ComponentGUIConsole             32
!define ComponentPDFDocs                64
!define ComponentHTMLDocs               128
!define ComponentTrayMonitor            1024

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
  StrCpy $LocalDirectorPassword ""

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

  ; Configuration Defaults

  StrCpy $ConfigClientName               "$HostName-fd"
  StrCpy $ConfigClientPort               9102
  StrCpy $ConfigClientMaxJobs            10
  ;StrCpy $ConfigClientPassword
  StrCpy $ConfigClientInstallService     "$OptService"
  StrCpy $ConfigClientStartService       "$OptStart"

  StrCpy $ConfigDirectorPort             9101

  StrCpy $ConfigMonitorName              "$HostName-mon"
  ;StrCpy $ConfigMonitorPassword

; PLUGINSDIR refers to temporary helper programs and not Bacula plugins!
  InitPluginsDir
  File "/oname=$PLUGINSDIR\openssl.exe"  "${SRC_DIR}\openssl.exe"
  File "/oname=$PLUGINSDIR\ssleay32-0.9.8.dll" "${SRC_DIR}\ssleay32-0.9.8.dll"
  File "/oname=$PLUGINSDIR\cryptoeay32-0.9.8.dll" "${SRC_DIR}\cryptoeay32-0.9.8.dll"        
;  File "/oname=$PLUGINSDIR\libeay32.dll" "${SRC_DIR}\libeay32.dll"
;  File "/oname=$PLUGINSDIR\ssleay32.dll" "${SRC_DIR}\ssleay32.dll"
  File "/oname=$PLUGINSDIR\sed.exe"      "${SRC_DIR}\sed.exe"

  !InsertMacro MUI_INSTALLOPTIONS_EXTRACT "InstallType.ini"
  !InsertMacro MUI_INSTALLOPTIONS_EXTRACT "WriteTemplates.ini"

  SetPluginUnload alwaysoff

; Set client password
  nsExec::Exec '"$PLUGINSDIR\openssl.exe" rand -base64 -out $PLUGINSDIR\pw.txt 33'
  pop $R0
  ${If} $R0 = 0
   FileOpen $R1 "$PLUGINSDIR\pw.txt" r
   IfErrors +4
     FileRead $R1 $R0
     ${StrTrimNewLines} $ConfigClientPassword $R0
     FileClose $R1
  ${EndIf}

  SetPluginUnload manual

; Set monitor password
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

    SetOutPath "$INSTDIR"
!if "${BUILD_TOOLS}" == "MinGW32"
    File "${SRC_DIR}\mingwm10.dll"
    File "${SRC_DIR}\pthreadGCE.dll"
    File "${SRC_DIR}\zlib1.dll"
    File "${SRC_DIR}\ssleay32.dll"
    File "${SRC_DIR}\libeay32.dll"
!endif
!if "${BUILD_TOOLS}" == "MinGW64"
    File "${SRC_DIR}\pthreadGCE.dll"
    File "${SRC_DIR}\cryptoeay32-0.9.8.dll"
    File "${SRC_DIR}\ssleay32-0.9.8.dll"
    File "${SRC_DIR}\zlib1.dll"
!endif
    File "${SRC_DIR}\bacula.dll"
    File "/oname=$INSTDIR\openssl.cnf" "${SRC_DIR}\openssl.cnf"
    File "${SRC_DIR}\openssl.exe"
    File "${SRC_DIR}\bsleep.exe"
    File "${SRC_DIR}\bsmtp.exe"
    File "${SRC_DIR}\expr64.exe"
    File "${SRC_DIR}\snooze.exe"

    CreateShortCut "$SMPROGRAMS\Bacula\Documentation\View Readme.lnk" "write.exe" '"$INSTDIR\Readme.txt"'

    StrCpy $CommonFilesDone 1
  ${EndIf}
FunctionEnd

Section "-Initialize"

  WriteRegStr   HKLM Software\Bacula InstallLocation "$INSTDIR"

  Call GetSelectedComponents
  Pop $R2
  WriteRegDWORD HKLM Software\Bacula Components $R2

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
  CreateDirectory "$INSTDIR\working"

  SetOutPath "$INSTDIR"
  File "..\..\..\LICENSE"
  Delete /REBOOTOK "$INSTDIR\License.txt"

; Output a series of SED commands to configure the .conf file(s)
  FileOpen $R1 $PLUGINSDIR\config.sed w
  FileWrite $R1 "s;@VERSION@;${VERSION};g$\r$\n"
  FileWrite $R1 "s;@DATE@;${__DATE__};g$\r$\n"
  FileWrite $R1 "s;@DISTNAME@;Windows;g$\r$\n"

  StrCpy $R2 ${BUILD_TOOLS}

  Call GetHostName
  Exch $R3
  Pop $R3

  FileWrite $R1 "s;@DISTVER@;$R2;g$\r$\n"

  ${StrRep} $R2 "$INSTDIR\working" "\" "\\\\"
  FileWrite $R1 's;@working_dir@;$R2;g$\r$\n'
  ${StrRep} $R2 "$INSTDIR\working" "\" "\\"
  FileWrite $R1 's;@working_dir_cmd@;$R2;g$\r$\n'

  ${StrRep} $R2 "$INSTDIR" "\" "\\\\"
  FileWrite $R1 's;@bin_dir@;$R2;g$\r$\n'
  ${StrRep} $R2 "$INSTDIR" "\" "\\"
  FileWrite $R1 's;@bin_dir_cmd@;$R2;g$\r$\n'

  ${StrRep} $R2 "$INSTDIR\plugins" "\" "\\\\"
  FileWrite $R1 's;@fdplugins_dir@;$R2;g$\r$\n'

  ${StrRep} $R2 "$INSTDIR" "\" "/"
  FileWrite $R1 "s;@BUILD_DIR@;$R2;g$\r$\n"

  FileWrite $R1 "s;@client_address@;$LocalHostAddress;g$\r$\n"

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
  ${If} "$ConfigDirectorName" != ""
    FileWrite $R1 "s;@director_name@;$ConfigDirectorName;g$\r$\n"
  ${EndIf}
  ${If} "$ConfigDirectorPort" != ""
    FileWrite $R1 "s;@director_port@;$ConfigDirectorPort;g$\r$\n"
  ${EndIf}
  ${If} "$ConfigDirectorPassword" != ""
    FileWrite $R1 "s;@director_password@;$ConfigDirectorPassword;g$\r$\n"
  ${EndIf}
  ${If} "$ConfigDirectorAddress" != ""
    FileWrite $R1 "s;@director_address@;$ConfigDirectorAddress;g$\r$\n"
  ${EndIf}
  ${If} "$ConfigMonitorName" != ""
    FileWrite $R1 "s;@monitor_name@;$ConfigMonitorName;g$\r$\n"
  ${EndIf}
  ${If} "$ConfigMonitorPassword" != ""
    FileWrite $R1 "s;@monitor_password@;$ConfigMonitorPassword;g$\r$\n"
  ${EndIf}

  FileClose $R1

  ${If} ${FileExists} "$OldInstallDir\bacula-fd.exe"
    nsExec::ExecToLog '"$OldInstallDir\bacula-fd.exe" /kill'     ; Shutdown any bacula that could be running
    Sleep 3000
    nsExec::ExecToLog '"$OldInstallDir\bacula-fd.exe" /remove'   ; Remove existing service
  ${EndIf}

SectionEnd

SectionGroup "Client" SecGroupClient

Section "File Service" SecFileDaemon
  SectionIn 1 2 3

  SetOutPath "$INSTDIR\plugins"
  File "${SRC_DIR}\exchange-fd.dll"

  SetOutPath "$INSTDIR"

  File "${SRC_DIR}\bacula-fd.exe"

  File "/oname=$PLUGINSDIR\bacula-fd.conf" "bacula-fd.conf.in"

  StrCpy $0 "$INSTDIR"
  StrCpy $1 bacula-fd.conf
  Call ConfigEditAndCopy

  StrCpy $0 bacula-fd
  StrCpy $1 "File Service"
  StrCpy $2 $ConfigClientInstallService
  StrCpy $3 $ConfigClientStartService

  Call InstallDaemon

  CreateShortCut "$SMPROGRAMS\Bacula\Configuration\Edit Client Configuration.lnk" "write.exe" '"$INSTDIR\bacula-fd.conf"'
SectionEnd

SectionGroupEnd

SectionGroup "Consoles" SecGroupConsoles

Section "Command Console" SecConsole
  SectionIn 1 2 3

  SetOutPath "$INSTDIR"

  File "${SRC_DIR}\bconsole.exe"
  Call InstallCommonFiles

  File "/oname=$PLUGINSDIR\bconsole.conf" "bconsole.conf.in"
  StrCpy $0 "$INSTDIR"
  StrCpy $1 bconsole.conf
  Call ConfigEditAndCopy

  CreateShortCut "$SMPROGRAMS\Bacula\bconsole.lnk" "$INSTDIR\bconsole.exe" '-c "$INSTDIR\bconsole.conf"' "$INSTDIR\bconsole.exe" 0
  CreateShortCut "$SMPROGRAMS\Bacula\Configuration\Edit Command Console Configuration.lnk" "write.exe" '"$INSTDIR\bconsole.conf"'

SectionEnd

Section "Bat Console" SecBatConsole
  SectionIn 1 2 3

  SetOutPath "$INSTDIR\bin32"

  Call InstallCommonFiles
  File "${SRC_DIR}\QtCore4.dll"
  File "${SRC_DIR}\QtGui4.dll"
  File "${SRC_DIR}\libgcc_s_dw2-1.dll"
  File "${SRC_DIR}\mingwm10.dll"
  File "${SRC_DIR}\ssleay32.dll"
  File "${SRC_DIR}\libeay32.dll"
  File "${SRC_DIR}\bat.exe"
  File "/oname=$INSTDIR\bin32\bacula.dll" "${SRC_DIR}\bacula32.dll"
  File "/oname=$INSTDIR\bin32\pthreadGCE.dll" "${SRC_DIR}\pthreadGCE32.dll"
  File "/oname=$INSTDIR\bin32\zlib1.dll" "${SRC_DIR}\zlib132.dll"

  File "/oname=$PLUGINSDIR\bat.conf" "bat.conf.in"
  StrCpy $0 "$INSTDIR\bin32"
  StrCpy $1 bat.conf
  Call ConfigEditAndCopy

  SetOutPath "$INSTDIR\help"
  File "${SRC_DIR}\help\*"
  SetOutPath "$INSTDIR"


  ; Create Start Menu entry
  CreateShortCut "$SMPROGRAMS\Bacula\Bat.lnk" "$INSTDIR\bin32\bat.exe" '-c "$INSTDIR\bin32\bat.conf"' "$INSTDIR\bin32\bat.exe" 0
  CreateShortCut "$SMPROGRAMS\Bacula\Configuration\Edit Bat Configuration.lnk" "write.exe" '"$INSTDIR\bin32\bat.conf"'
  SetOutPath "$INSTDIR"
SectionEnd

Section "Tray Monitor" SecTrayMonitor
  SectionIn 1 2 3

  SetOutPath "$INSTDIR\bin32"

  Call InstallCommonFiles
  File "${SRC_DIR}\QtCore4.dll"
  File "${SRC_DIR}\QtGui4.dll"
  File "${SRC_DIR}\libgcc_s_dw2-1.dll"
  File "${SRC_DIR}\mingwm10.dll"
  File "${SRC_DIR}\ssleay32.dll"
  File "${SRC_DIR}\libeay32.dll"
  File "${SRC_DIR}\bacula-tray-monitor.exe"
  File "/oname=$INSTDIR\bin32\bacula.dll" "${SRC_DIR}\bacula32.dll"
  File "/oname=$INSTDIR\bin32\pthreadGCE.dll" "${SRC_DIR}\pthreadGCE32.dll"
  File "/oname=$INSTDIR\bin32\zlib1.dll" "${SRC_DIR}\zlib132.dll"

  File "/oname=$PLUGINSDIR\tray-monitor.conf" "tray-monitor.conf.in"
  StrCpy $0 "$INSTDIR\bin32"
  StrCpy $1 tray-monitor.conf
  Call ConfigEditAndCopy

  ; Create Start Menu entry
  CreateShortCut "$SMPROGRAMS\Bacula\TrayMonitor.lnk" "$INSTDIR\bin32\bacula-tray-monitor.exe" '-c "$INSTDIR\bin32\tray-monitor.conf"' "$INSTDIR\bin32\bacula-tray-monitor.exe" 0
  CreateShortCut "$SMPROGRAMS\Bacula\Configuration\Edit Tray Monitor Configuration.lnk" "write.exe" '"$INSTDIR\bin32\tray-monitor.conf"'
  SetOutPath "$INSTDIR"

SectionEnd

; Deleted because wxconsole is deprecated
;Section "Graphical Console" SecWxConsole
;  SectionIn 1 2 3
  
;  SetOutPath "$INSTDIR"
;
;SectionEnd

SectionGroupEnd


Section "-Finish"
  Push $R0

  ${If} $OsIsNT = 1
;   nsExec::ExecToLog 'cmd.exe /C echo Y|cacls "$INSTDIR" /T /G SYSTEM:F Administrators:F'
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
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Bacula" "URLUpdateInfo" "http://www.bacula.org"
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Bacula" "URLInfoAbout" "http://www.bacula.org"
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Bacula" "HelpLink" "http://www.bacula.org?page=support"
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Bacula" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  CreateShortCut "$SMPROGRAMS\Bacula\Uninstall Bacula.lnk" "$INSTDIR\Uninstall.exe" "" "$INSTDIR\Uninstall.exe" 0
  Pop $R0
SectionEnd

; Extra Page descriptions

LangString DESC_SecFileDaemon ${LANG_ENGLISH} "Install Bacula File Daemon on this system."
LangString DESC_SecConsole ${LANG_ENGLISH} "Install command console program on this system."
LangString DESC_SecBatConsole ${LANG_ENGLISH} "Install Bat graphical console program on this system."
LangString DESC_SecTrayMonitor ${LANG_ENGLISH} "Install Tray Monitor graphical program on this system."

LangString TITLE_ConfigPage1 ${LANG_ENGLISH} "Configuration"
LangString SUBTITLE_ConfigPage1 ${LANG_ENGLISH} "Set installation configuration."

LangString TITLE_ConfigPage2 ${LANG_ENGLISH} "Configuration (continued)"
LangString SUBTITLE_ConfigPage2 ${LANG_ENGLISH} "Set installation configuration."

LangString TITLE_InstallType ${LANG_ENGLISH} "Installation Type"
LangString SUBTITLE_InstallType ${LANG_ENGLISH} "Choose installation type."

LangString TITLE_WriteTemplates ${LANG_ENGLISH} "Create Templates"
LangString SUBTITLE_WriteTemplates ${LANG_ENGLISH} "Create a resource template for inclusion in the Director's configuration file."

!InsertMacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !InsertMacro MUI_DESCRIPTION_TEXT ${SecFileDaemon} $(DESC_SecFileDaemon)
  !InsertMacro MUI_DESCRIPTION_TEXT ${SecConsole} $(DESC_SecConsole)
  !InsertMacro MUI_DESCRIPTION_TEXT ${SecBatConsole} $(DESC_SecBatConsole)
  !InsertMacro MUI_DESCRIPTION_TEXT ${SecTrayMonitor} $(DESC_SecTrayMonitor)
!InsertMacro MUI_FUNCTION_DESCRIPTION_END

; Uninstall section

UninstallText "This will uninstall Bacula. Click Uninstall to continue."

Section "Uninstall"
  ; Shutdown any baculum that could be running
  nsExec::ExecToLog '"$INSTDIR\bacula-fd.exe" /kill'
  Sleep 3000

; ReadRegDWORD $R0 HKLM "Software\Bacula" "Service_Bacula-fd"
  ; Remove bacula service
  nsExec::ExecToLog '"$INSTDIR\bacula-fd.exe" /remove'
  nsExec::ExecToLog '"$INSTDIR\plugins\exchange-fd.dll" /remove'
  
  ; remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Bacula"
  DeleteRegKey HKLM "Software\Bacula"

  ; remove start menu items
  SetShellVarContext all
  Delete /REBOOTOK "$SMPROGRAMS\Bacula\*"
  RMDir "$SMPROGRAMS\Bacula"

  ; remove files and uninstaller (preserving config for now)
  Delete /REBOOTOK "$INSTDIR\doc\*"
  Delete /REBOOTOK "$INSTDIR\*"
  Delete /REBOOTOK "$INSTDIR\help\*"

  ; Check for existing installation
  MessageBox MB_YESNO|MB_ICONQUESTION \
  "Would you like to delete the current configuration files and the working state file?" IDNO NoDel
    Delete /REBOOTOK "$INSTDIR\*"
    Delete /REBOOTOK "$INSTDIR\bin32\*"
    Delete /REBOOTOK "$INSTDIR\working\*"
    Delete /REBOOTOK "$INSTDIR\plugins\*"
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
    RMDir "$INSTDIR\plugins"
    RMDir "$INSTDIR\working"
    RMDir "$INSTDIR\bin32"
    RMDir "$INSTDIR"
NoDel:

  ; remove directories used
  RMDir "$INSTDIR\doc"
  RMDir "$INSTDIR\help"
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
    nsExec::ExecToLog '"$INSTDIR\$0.exe" /kill'
    sleep 3000
    nsExec::ExecToLog '"$INSTDIR\$0.exe" /remove'
    nsExec::ExecToLog '"$INSTDIR\$0.exe" /install -c "$INSTDIR\$0.conf"'

    ${If} $OsIsNT <> 1
      File "Start.bat"
      File "Stop.bat"
    ${EndIf}

    ; Start the service?

    ${If} $3 = 1  
      ${If} $OsIsNT = 1
        nsExec::ExecToLog 'net start $0'
      ${Else}
        Exec '"$INSTDIR\$0.exe" /service -c "$INSTDIR\$0.conf"'
      ${EndIf}
    ${EndIf}
  ${Else}
    CreateShortCut "$SMPROGRAMS\Bacula\Start $1.lnk" "$INSTDIR\$0.exe" '-c "$INSTDIR\$0.conf"' "$INSTDIR\$0.exe" 0
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

Function GetSelectedComponents
  Push $R0
  StrCpy $R0 0
  ${If} ${SectionIsSelected} ${SecFileDaemon}
    IntOp $R0 $R0 | ${ComponentFile}
  ${EndIf}
  ${If} ${SectionIsSelected} ${SecConsole}
    IntOp $R0 $R0 | ${ComponentTextConsole}
  ${EndIf}
  ${If} ${SectionIsSelected} ${SecBatConsole}
    IntOp $R0 $R0 | ${ComponentBatConsole}
  ${EndIf}
  ${If} ${SectionIsSelected} ${SecTrayMonitor}
    IntOp $R0 $R0 | ${ComponentTrayMonitor}
  ${EndIf}
  Exch $R0
FunctionEnd

Function PageComponentsShow

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
    IntOp $R1 $PreviousComponents & ${ComponentTrayMonitor}
    ${If} $R1 <> 0
      !InsertMacro SelectSection ${SecTrayMonitor}
      !InsertMacro SetSectionFlag ${SecTrayMonitor} ${SF_RO}
    ${Else}
      !InsertMacro UnselectSection ${SecTrayMonitor}
      !InsertMacro ClearSectionFlag ${SecTrayMonitor} ${SF_RO}
    ${EndIf}
  ${EndIf}
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
    IntOp $R1 $NewComponents & ${ComponentTrayMonitor}
    ${If} $R1 <> 0
      !InsertMacro SetSectionFlag ${SecTrayMonitor} ${SF_BOLD}
    ${Else}
      !InsertMacro ClearSectionFlag ${SecTrayMonitor} ${SF_BOLD}
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
