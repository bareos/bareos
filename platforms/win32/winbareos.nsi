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

# Needed for bconsole and bat:
Var DirectorAddress       #XXX_REPLACE_WITH_HOSTNAME_XXX
Var DirectorPassword      #XXX_REPLACE_WITH_DIRECTOR_PASSWORD_XXX
Var DirectorName

# Needed for tray monitor:

# can stay like it is if we dont monitor any director -> Default
Var DirectorMonitorPassword #XXX_REPLACE_WITH_DIRECTOR_MONITOR_PASSWORD_XXX

# this is the one we need to make sure it is the same like configured in the fd
#Var ClientMonitorPassword   #XXX_REPLACE_WITH_FD_MONITOR_PASSWORD_XXX


# generated configuration snippet for bareos director config  (client ressource)
Var ConfigSnippet


Var dialog
Var hwnd

# keep the configuration files also when running silently?

Var SilentKeepConfig

!include "LogicLib.nsh"
!include "FileFunc.nsh"
!include "Sections.nsh"
!include "StrFunc.nsh"
!include "WinMessages.nsh"
!include "nsDialogs.nsh"
!include "x64.nsh"
!include "WinVer.nsh"

# call functions once to have them included
${StrCase}
${StrTrimNewLines}

; MUI 1.67 compatible ------
!include "MUI.nsh"

; MUI Settings
!define MUI_ABORTWARNING
!define MUI_ICON   "bareos.ico"
!define MUI_UNICON "bareos.ico"
#!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
#!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"

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

; Custom für Abfragen benötigter Parameter für den Client
Page custom getClientParameters

; Custom für Abfragen benötigter Parameter für den Zugriff auf director
Page custom getDirectorParameters

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

  StrCmp $SilentKeepConfig "yes" keep

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
      MessageBox MB_OK|MB_ICONINFORMATION \
        "New config file stored $APPDATA\${PRODUCT_NAME}\${fname}.new" \
        /SD IDOK
  ${Else}
    Rename "$PLUGINSDIR\${fname}" "$APPDATA\${PRODUCT_NAME}\${fname}"
  ${EndIf}
 CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Edit ${fname}.lnk" "write.exe" '"$APPDATA\${PRODUCT_NAME}\${fname}"'


# for traymonitor.conf, set access and ownership to users
${If} ${fname} == "tray-monitor.conf"

    # disable file access inheritance
    AccessControl::DisableFileInheritance "$APPDATA\${PRODUCT_NAME}\${fname}"
    Pop $R0
    DetailPrint `AccessControl result: $R0`
    ${If} $R0 == error
       Pop $R0
       DetailPrint `AccessControl error: $R0`
    ${EndIf}

    # set file owner to administrator
    AccessControl::SetFileOwner "$APPDATA\${PRODUCT_NAME}\${fname}" "(S-1-5-32-545)"  # user
    Pop $R0
    DetailPrint `AccessControl result: $R0`
    ${If} $R0 == error
       Pop $R0
       DetailPrint `AccessControl error: $R0`
    ${EndIf}

    # set fullaccess only for administrators (S-1-5-32-544)
    AccessControl::ClearOnFile "$APPDATA\${PRODUCT_NAME}\${fname}" "(S-1-5-32-545)" "FullAccess"
    Pop $R0
    DetailPrint `AccessControl result: $R0`
    ${If} $R0 == error
       Pop $R0
       DetailPrint `AccessControl error: $R0`
    ${EndIf}


# all other config files admin owner and only access
${Else}
    # disable file access inheritance
    AccessControl::DisableFileInheritance "$APPDATA\${PRODUCT_NAME}\${fname}"
    Pop $R0
    DetailPrint `AccessControl result: $R0`
    ${If} $R0 == error
       Pop $R0
       DetailPrint `AccessControl error: $R0`
    ${EndIf}

    # set file owner to administrator
    AccessControl::SetFileOwner "$APPDATA\${PRODUCT_NAME}\${fname}" "(S-1-5-32-544)"  # administratoren
    Pop $R0
    DetailPrint `AccessControl result: $R0`
    ${If} $R0 == error
       Pop $R0
       DetailPrint `AccessControl error: $R0`
    ${EndIf}

    # set fullaccess only for administrators (S-1-5-32-544)
    AccessControl::ClearOnFile "$APPDATA\${PRODUCT_NAME}\${fname}" "(S-1-5-32-544)" "FullAccess"
    Pop $R0
    DetailPrint `AccessControl result: $R0`
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
  KillProcWMI::KillProc "bareos-tray-monitor.exe"
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
  FileWrite $R1 "s#XXX_REPLACE_WITH_CLIENT_PASSWORD_XXX#$ClientPassword#g$\r$\n"
  FileWrite $R1 "s#XXX_REPLACE_WITH_CLIENT_MONITOR_PASSWORD_XXX#$ClientMonitorPassword#g$\r$\n"

  FileWrite $R1 "s#XXX_REPLACE_WITH_HOSTNAME_XXX#$HostName#g$\r$\n"

  FileWrite $R1 "s#XXX_REPLACE_WITH_BASENAME_XXX-fd#$ClientName#g$\r$\n"
  FileWrite $R1 "s#XXX_REPLACE_WITH_BASENAME_XXX-dir#$DirectorName#g$\r$\n"
  FileWrite $R1 "s#XXX_REPLACE_WITH_BASENAME_XXX-mon#$HostName-mon#g$\r$\n"

  FileClose $R1

  nsExec::ExecToLog '$PLUGINSDIR\sed.exe -f "$PLUGINSDIR\config.sed" -i-template "$PLUGINSDIR\bareos-fd.conf"'
  nsExec::ExecToLog '$PLUGINSDIR\sed.exe -f "$PLUGINSDIR\config.sed" -i-template "$PLUGINSDIR\tray-monitor.conf"'

  #Delete config.sed


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
#
SectionEnd

Section "Bareos Client (FileDaemon) and base libs" SEC_CLIENT
SectionIn 1 2 3

  SetShellVarContext all
# TODO: only do this if the file exists
#  nsExec::ExecToLog '"$INSTDIR\bareos-fd.exe" /kill'
#  sleep 3000
#  nsExec::ExecToLog '"$INSTDIR\bareos-fd.exe" /remove'

  SetOutPath "$INSTDIR"
  SetOverwrite ifnewer
  CreateDirectory "$SMPROGRAMS\${PRODUCT_NAME}"
  CreateDirectory "$SMPROGRAMS\${PRODUCT_NAME}\Plugins"
  CreateDirectory "$APPDATA\${PRODUCT_NAME}"
  File "bareos-fd.exe"
  File "libbareos.dll"
  File "libbareosfind.dll"
  File "libcrypto-8.dll"
  File "libgcc_s_*-1.dll"
  File "libssl-8.dll"
  File "libstdc++-6.dll"
  File "pthreadGCE2.dll"
  File "zlib1.dll"
  File "liblzo2-2.dll"
  File "libfastlz.dll"

# for password generation
  File "openssl.exe"
  File "sed.exe"

#  File "bareos-fd.conf"
  !insertmacro InstallConfFile bareos-fd.conf
SectionEnd

Section /o "Bareos FileDaemon Plugins " SEC_PLUGINS
SectionIn 1 2 3

  SetShellVarContext all
  SetOutPath "$INSTDIR\Plugins"
  SetOverwrite ifnewer
  File "bpipe-fd.dll"
  File "mssqlvdi-fd.dll"
SectionEnd

Section /o "Text Console (bconsole)" SEC_BCONSOLE
SectionIn 2

  SetShellVarContext all
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\bconsole.lnk" "$INSTDIR\bconsole.exe" '-c "$APPDATA\${PRODUCT_NAME}\bconsole.conf"'

  File "bconsole.exe"
#  File "libbareos.dll"
#  File "libcrypto-8.dll"
#  File "libgcc_s_*-1.dll"
  File "libhistory6.dll"
  File "libreadline6.dll"
#  File "libssl-8.dll"
#  File "libstdc++-6.dll"
  File "libtermcap-0.dll"
#  File "pthreadGCE2.dll"
#  File "zlib1.dll"
#
 !insertmacro InstallConfFile "bconsole.conf"
#  File "bconsole.conf

SectionEnd

Section /o "Tray-Monitor" SEC_TRAYMON
SectionIn 1 2


  SetShellVarContext all
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\bareos-tray-monitor.lnk" "$INSTDIR\bareos-tray-monitor.exe" '-c "$APPDATA\${PRODUCT_NAME}\tray-monitor.conf"'

# autostart
  CreateShortCut "$SMSTARTUP\bareos-tray-monitor.lnk" "$INSTDIR\bareos-tray-monitor.exe" '-c "$APPDATA\${PRODUCT_NAME}\tray-monitor.conf"'

  File "bareos-tray-monitor.exe"
#  File "libbareos.dll"
#  File "libcrypto-8.dll"
#  File "libgcc_s_*-1.dll"
  File "libpng15-15.dll"
#  File "libssl-8.dll"
#  File "libstdc++-6.dll"
#  File "pthreadGCE2.dll"
  File "QtCore4.dll"
  File "QtGui4.dll"
#  File "zlib1.dll"


  !insertmacro InstallConfFile "tray-monitor.conf"
#  File "tray-monitor.conf"

SectionEnd


Section /o "Qt Console (BAT)" SEC_BAT
SectionIn 2

  SetShellVarContext all
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\BAT.lnk" "$INSTDIR\bat.exe" '-c "$APPDATA\${PRODUCT_NAME}\bat.conf"'
  CreateShortCut "$DESKTOP\BAT.lnk" "$INSTDIR\bat.exe" '-c "$APPDATA\${PRODUCT_NAME}\bat.conf"'

  File "bat.exe"
#  File "libbareos.dll"
#  File "libcrypto-8.dll"
#  File "libgcc_s_*-1.dll"
  File "libpng15-15.dll"
#  File "libssl-8.dll"
#  File "libstdc++-6.dll"
#  File "pthreadGCE2.dll"
  File "QtCore4.dll"
  File "QtGui4.dll"
#  File "zlib1.dll"

  !insertmacro InstallConfFile "bat.conf"
#  File "bat.conf"

SectionEnd

Section "Open Firewall for Client" SEC_FIREWALL
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


; Section descriptions
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_CLIENT} "Installs the Bareos File Daemon and required Files"
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_PLUGINS} "Installs the Bareos File Daemon Plugins"
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_BCONSOLE} "Installs the CLI client console (bconsole)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_TRAYMON} "Installs the tray Icon to monitor the Bareos client"
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_BAT} "Installs the Qt Console (BAT)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_FIREWALL} "Opens Port 9102/TCP for bareos-fd.exe (Client program) in the Windows Firewall"
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

SectionEnd


Section -StartDaemon
  nsExec::ExecToLog "net start bareos-fd"
  ${If} ${SectionIsSelected} ${SEC_TRAYMON}
    MessageBox MB_OK|MB_ICONINFORMATION "The tray monitor will be started automatically on next login" /SD IDOK
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
# check if we are installing on 64Bit, then do some settings
  ${If} ${RunningX64} # 64Bit OS
    ${If} ${BIT_WIDTH} == '32'
      MessageBox MB_OK|MB_ICONSTOP "You are running a 32 Bit installer on a 64 Bit OS.$\r$\nPlease use the 64 Bit installer."
      Abort
    ${EndIf}
    StrCpy $INSTDIR "$PROGRAMFILES64\${PRODUCT_NAME}"
    SetRegView 64
    ${EnableX64FSRedirection}
  ${Else} # 32Bit OS
    ${If} ${BIT_WIDTH} == '64'
      MessageBox MB_OK|MB_ICONSTOP "You are running a 64 Bit installer on a 32 Bit OS.$\r$\nPlease use the 32 Bit installer."
      Abort
    ${EndIf}
  ${EndIf}



# check if software is already installed
  ClearErrors
  ReadRegStr $2 ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayName"
  ReadRegStr $0 ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayVersion"
  StrCmp $2 "" +3
  MessageBox MB_OK|MB_ICONSTOP "${PRODUCT_NAME} version $0 $\r$\nseems to be already installed on your system.$\r$\nPlease uninstall first."
  Abort


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
                    $\r$\n\
                    [/DIRECTORADDRESS=Network Address of the Director (for bconsole or BAT)] $\r$\n\
                    [/DIRECTORPASSWORD=Password to access Director]$\r$\n\
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

  ${GetOptions} $cmdLineParams "/DIRECTORADDRESS=" $DirectorAddress
  ClearErrors

  ${GetOptions} $cmdLineParams "/DIRECTORPASSWORD=" $DirectorPassword
  ClearErrors

  ${GetOptions} $cmdLineParams "/DIRECTORNAME=" $DirectorName
  ClearErrors

  StrCpy $SilentKeepConfig "yes"
  ${GetOptions} $cmdLineParams "/SILENTKEEPCONFIG"  $R0
  IfErrors 0 +2         # error is set if NOT found
    StrCpy $SilentKeepConfig "no"
  ClearErrors

##
##   MessageBox MB_YESNO|MB_ICONQUESTION \
##       "SilentKeepConfig is $SilentKeepConfig"
##


  InitPluginsDir
  File "/oname=$PLUGINSDIR\clientdialog.ini"    "clientdialog.ini"
  File "/oname=$PLUGINSDIR\directordialog.ini"  "directordialog.ini"
  File "/oname=$PLUGINSDIR\openssl.exe"  	      "openssl.exe"
  File "/oname=$PLUGINSDIR\sed.exe"  	         "sed.exe"
  File "/oname=$PLUGINSDIR\libcrypto-8.dll" 	   "libcrypto-8.dll"

  File /nonfatal "/oname=$PLUGINSDIR\libgcc_s_sjlj-1.dll" "libgcc_s_sjlj-1.dll"
  File /nonfatal "/oname=$PLUGINSDIR\libgcc_s_seh-1.dll"  "libgcc_s_seh-1.dll"

  File "/oname=$PLUGINSDIR\libssl-8.dll" 	      "libssl-8.dll"
  File "/oname=$PLUGINSDIR\libstdc++-6.dll" 	   "libstdc++-6.dll"
  File "/oname=$PLUGINSDIR\zlib1.dll" 	         "zlib1.dll"

  File "/oname=$PLUGINSDIR\bareos-fd.conf"     "bareos-fd.conf"
  File "/oname=$PLUGINSDIR\bconsole.conf"      "bconsole.conf"
  File "/oname=$PLUGINSDIR\bat.conf"           "bat.conf"
  File "/oname=$PLUGINSDIR\tray-monitor.conf"  "tray-monitor.conf"

# make first section mandatory
  SectionSetFlags ${SEC_CLIENT}  17 # SF_SELECTED & SF_RO
#  SectionSetFlags ${SEC_BCONSOLE}  ${SF_SELECTED} # SF_SELECTED
SectionSetFlags ${SEC_TRAYMON}  ${SF_SELECTED} # SF_SELECTED

# find out the computer name
  Call GetComputerName
  Pop $HostName

  Call GetHostName
  Pop $LocalHostAddress

#  MessageBox MB_OK "Hostname: $HostName"
#  MessageBox MB_OK "LocalHostAddress: $LocalHostAddress"

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



#  MessageBox MB_OK "RandomPassword: $ClientPassword"
#  MessageBox MB_OK "RandomPassword: $ClientMonitorPassword"



# if the variables are not empty (because of cmdline params),
# dont set them with our own logic but leave them as they are
  strcmp $ClientName     "" +1 +2
  StrCpy $ClientName    "$HostName-fd"
  strcmp $ClientAddress "" +1 +2
  StrCpy $ClientAddress "$HostName"
  strcmp $DirectorName   "" +1 +2
  StrCpy $DirectorName  "bareos-dir"
  strcmp $DirectorAddress  "" +1 +2
  StrCpy $DirectorAddress  "bareos-dir.example.com"
  strcmp $DirectorPassword "" +1 +2
  StrCpy $DirectorPassword "DIRECTORPASSWORD"
FunctionEnd




#
# Client Configuration Dialog
#
Function getClientParameters
  Push $R0

# prefill the dialog fields with our passwords and other
# information

  WriteINIStr "$PLUGINSDIR\clientdialog.ini" "Field 2" "state" $ClientName
  WriteINIStr "$PLUGINSDIR\clientdialog.ini" "Field 3" "state" $DirectorName
  WriteINIStr "$PLUGINSDIR\clientdialog.ini" "Field 4" "state" $ClientPassword
  WriteINIStr "$PLUGINSDIR\clientdialog.ini" "Field 14" "state" $ClientMonitorPassword
  WriteINIStr "$PLUGINSDIR\clientdialog.ini" "Field 5" "state" $ClientAddress
#  WriteINIStr "$PLUGINSDIR\clientdialog.ini" "Field 7" "state" "Director console password"


${If} ${SectionIsSelected} ${SEC_CLIENT}
  InstallOptions::dialog $PLUGINSDIR\clientdialog.ini
  Pop $R0
  ReadINIStr  $ClientName             "$PLUGINSDIR\clientdialog.ini" "Field 2" "state"
  ReadINIStr  $DirectorName            "$PLUGINSDIR\clientdialog.ini" "Field 3" "state"
  ReadINIStr  $ClientPassword          "$PLUGINSDIR\clientdialog.ini" "Field 4" "state"
  ReadINIStr  $ClientMonitorPassword   "$PLUGINSDIR\clientdialog.ini" "Field 14" "state"
  ReadINIStr  $ClientAddress           "$PLUGINSDIR\clientdialog.ini" "Field 5" "state"
${EndIf}
#  MessageBox MB_OK "$ClientName$\r$\n$ClientPassword$\r$\n$ClientMonitorPassword "



  Pop $R0
FunctionEnd

#
# Director Configuration Dialog (for bconsole and bat configuration)
#
Function getDirectorParameters
  Push $R0
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
  Pop $R0
FunctionEnd

#
# Display auto-created snippet to be added to director config
#
Function displayDirconfSnippet

#
# write client config snippet for director
#
# Multiline text edits cannot be created before but have to be created at runtime.
# see http://nsis.sourceforge.net/NsDialogs_FAQ#How_to_create_a_multi-line_edit_.28text.29_control

  StrCpy $ConfigSnippet "Client {$\r$\n  \
                              Name = $ClientName$\r$\n  \
                              Address = $ClientAddress$\r$\n  \
                              Password = $\"$ClientPassword$\"$\r$\n  \
                              # uncomment the following if using bacula $\r$\n  \
                              # Catalog = $\"MyCatalog$\"$\r$\n  \
                           }$\r$\n"



  nsDialogs::Create 1018
  Pop $dialog
  ${NSD_CreateGroupBox} 0 0 100% 100% "Add this Client Ressource to your Bareos Director Configuration"
      Pop $hwnd

  nsDialogs::CreateControl EDIT \
      "${__NSD_Text_STYLE}|${WS_VSCROLL}|${ES_MULTILINE}|${ES_WANTRETURN}" \
      "${__NSD_Text_EXSTYLE}" \
      10 15 95% 90% \
      $ConfigSnippet
      Pop $hwnd

  nsDialogs::Show

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
    StrCpy $INSTDIR "$PROGRAMFILES64\${PRODUCT_NAME}"
    SetRegView 64
    ${EnableX64FSRedirection}
  ${EndIf}

  SetShellVarContext all
# uninstall service
  nsExec::ExecToLog '"$INSTDIR\bareos-fd.exe" /kill'
  sleep 3000
  nsExec::ExecToLog '"$INSTDIR\bareos-fd.exe" /remove'


#  KillProcWMI::KillProc "bareos-fd.exe"
# kill tray monitor
  KillProcWMI::KillProc "bareos-tray-monitor.exe"


##
##   MessageBox MB_YESNO|MB_ICONQUESTION \
##       "SilentKeepConfig is $SilentKeepConfig"
##


  StrCmp $SilentKeepConfig "yes" ConfDeleteSkip # keep if silent and  $SilentKeepConfig is yes

  MessageBox MB_YESNO|MB_ICONQUESTION \
    "Do you want to keep the existing configuration files?" /SD IDNO IDYES ConfDeleteSkip

  Delete "$APPDATA\${PRODUCT_NAME}\bareos-fd.conf"
  Delete "$APPDATA\${PRODUCT_NAME}\tray-monitor.conf"
  Delete "$APPDATA\${PRODUCT_NAME}\bconsole.conf"
  Delete "$APPDATA\${PRODUCT_NAME}\bat.conf"

ConfDeleteSkip:
  Delete "$APPDATA\${PRODUCT_NAME}\bareos-fd.conf.old"
  Delete "$APPDATA\${PRODUCT_NAME}\tray-monitor.conf.old"
  Delete "$APPDATA\${PRODUCT_NAME}\bconsole.conf.old"
  Delete "$APPDATA\${PRODUCT_NAME}\bat.conf.old"

  RMDir  "$APPDATA\${PRODUCT_NAME}"

  Delete "$INSTDIR\${PRODUCT_NAME}.url"
  Delete "$INSTDIR\uninst.exe"
  Delete "$INSTDIR\bareos-tray-monitor.exe"
  Delete "$INSTDIR\bat.exe"
  Delete "$INSTDIR\bareos-fd.exe"
  Delete "$INSTDIR\bconsole.exe"
  Delete "$INSTDIR\Plugins\bpipe-fd.dll"
  Delete "$INSTDIR\Plugins\mssqlvdi-fd.dll"
  Delete "$INSTDIR\libbareos.dll"
  Delete "$INSTDIR\libbareosfind.dll"
  Delete "$INSTDIR\libcrypto-8.dll"
  Delete "$INSTDIR\libgcc_s_*-1.dll"
  Delete "$INSTDIR\libhistory6.dll"
  Delete "$INSTDIR\libreadline6.dll"
  Delete "$INSTDIR\libssl-8.dll"
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

  Delete "$INSTDIR\*template"

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
  RMDir "$SMPROGRAMS\${PRODUCT_NAME}\Plugins"
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
  ${Else}
    DetailPrint  "Closing Firewall, OS is < Win7"
    DetailPrint  "netsh firewall delete portopening protocol=TCP port=9102 name=$\"Bareos backup client (bareos-fd) access$\""
    nsExec::Exec "netsh firewall delete portopening protocol=TCP port=9102 name=$\"Bareos backup client (bareos-fd) access$\""
  ${EndIf}

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
