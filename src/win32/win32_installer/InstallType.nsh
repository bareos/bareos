Function EnterInstallType
  Push $R0
  Push $R1
  Push $R2

  ; Check if this is an upgrade by looking for an uninstaller configured 
  ; in the registry.
  ReadRegStr $R0 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Bacula" "UninstallString"

  ${If} "$R0" != ""
    ; Check registry for new installer
    ReadRegStr $R1 HKLM "Software\Bacula" "InstallLocation"
    ${If} "$R1" != ""
      ; New Installer 
      StrCpy $OldInstallDir $R1
      StrCpy $InstallType ${UpgradeInstall}

      SetShellVarContext all

      StrCpy $R1 "$APPDATA\Bacula"
      StrCpy $R2 "$INSTDIR\Doc"

      ReadRegDWORD $PreviousComponents HKLM "Software\Bacula" "Components"

      WriteINIStr "$PLUGINSDIR\InstallType.ini" "Field 1" "Text" "A previous installation has been found in $OldInstallDir.  Please choose the installation type for any additional components you select."
      WriteINIStr "$PLUGINSDIR\InstallType.ini" "Field 5" "Text" "The configuration files for additional components will be generated using defaults applicable to most installations."
      WriteINIStr "$PLUGINSDIR\InstallType.ini" "Field 6" "Text" "You have more options, but you will have to manually edit your bacula-fd.conf file before Bacula will work."

      ReadRegDWORD $ConfigDirectorDB HKLM Software\Bacula Database

      ${If} $ConfigDirectorDB = 0
        IntOp $R0 $PreviousComponents & ${ComponentDirector}
        ${If} $R0 <> 0
          StrCpy $ConfigDirectorDB 1
        ${EndIf}
      ${EndIf}
    ${Else}
      ; Processing Upgrade - Get Install Directory
      ${StrRep} $R0 $R0 '"' ''
      ${GetParent} $R0 $OldInstallDir

      ; Old Installer 
      StrCpy $InstallType ${MigrateInstall}
      StrCpy $R1 "$OldInstallDir\bin"
      StrCpy $R2 "$OldInstallDir\Doc"

      WriteINIStr "$PLUGINSDIR\InstallType.ini" "Field 1" "Text" "An old installation has been found in $OldInstallDir.  The Configuration will be migrated.  Please choose the installation type for any additional components you select."
      WriteINIStr "$PLUGINSDIR\InstallType.ini" "Field 5" "Text" "The software will be installed in the default directory $\"C:\Program Files\Bacula$\".  The configuration files for additional components will be generated using defaults applicable to most installations."
      WriteINIStr "$PLUGINSDIR\InstallType.ini" "Field 6" "Text" "You have more options, but you will have to manually edit your bacula-fd.conf file before Bacula will work."
    ${EndIf}
  ${Else}
    ; New Install
    StrCpy $InstallType ${NewInstall}
    WriteINIStr "$PLUGINSDIR\InstallType.ini" "Field 5" "Text" "The software will be installed in the default directory $\"C:\Program Files\Bacula$\".  The configuration files will be generated using defaults applicable to most installations."
    WriteINIStr "$PLUGINSDIR\InstallType.ini" "Field 6" "Text" "You have more options, but you will have to manually edit your bacula-fd.conf file before Bacula will work."
  ${EndIf}

  ${If} $InstallType <> ${NewInstall}
  ${AndIf} $PreviousComponents = 0
    ${If} ${FileExists} "$R1\bacula-fd.conf"
      IntOp $PreviousComponents $PreviousComponents | ${ComponentFile}
    ${EndIf}
    ${If} ${FileExists} "$R1\bconsole.conf"
      IntOp $PreviousComponents $PreviousComponents | ${ComponentTextConsole}
    ${EndIf}
    ${If} ${FileExists} "$R1\bat.conf"
      IntOp $PreviousComponents $PreviousComponents | ${ComponentBatConsole}
    ${EndIf}
    ${If} ${FileExists} "$R2\main.pdf"
      IntOp $PreviousComponents $PreviousComponents | ${ComponentPDFDocs}
    ${EndIf}
  ${EndIf}

  !InsertMacro MUI_HEADER_TEXT "$(TITLE_InstallType)" "$(SUBTITLE_InstallType)"
  !InsertMacro MUI_INSTALLOPTIONS_INITDIALOG "InstallType.ini"
  Pop $HDLG ;HWND of dialog

  !insertmacro MUI_INSTALLOPTIONS_SHOW

  ; Process Results

  !insertmacro MUI_INSTALLOPTIONS_READ $R0 "InstallType.ini" "Field 3" "State"

  ${If} $R0 = 1
    StrCpy $AutomaticInstall 1
  ${Else}
    StrCpy $AutomaticInstall 0
  ${EndIf}

  Pop $R2
  Pop $R1
  Pop $R0
FunctionEnd
