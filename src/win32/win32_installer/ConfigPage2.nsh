Function EnterConfigPage2
  IntOp $R0 $NewComponents & ${ComponentsRequiringUserConfig}

  ${If} $R0 = 0
    Abort
  ${EndIf}

  FileOpen $R5 "$PLUGINSDIR\ConfigPage2.ini" w

  StrCpy $R6 1  ; Field Number
  StrCpy $R7 0  ; Top

  IntOp $R0 $NewComponents & ${ComponentDirector}
  ${If} $R0 <> 0
    ${If} $AutomaticInstall = 1
      IntOp $R8 $R7 + 54
    ${Else}
      IntOp $R8 $R7 + 92
    ${EndIf}
    FileWrite $R5 '[Field $R6]$\r$\nType="GroupBox"$\r$\nText="Director"$\r$\nLeft=0$\r$\nTop=$R7$\r$\nRight=300$\r$\nBottom=$R8$\r$\n$\r$\n'
  ${Else}
    IntOp $R0 $NewComponents & ${ComponentsTextAndGuiConsoles}
    ${If} $R0 <> 0
      IntOp $R8 $R7 + 54
    ${Else}
      IntOp $R8 $R7 + 26
    ${EndIf}
    FileWrite $R5 '[Field $R6]$\r$\nType="GroupBox"$\r$\nText="Enter Director Information"$\r$\nLeft=0$\r$\nTop=$R7$\r$\nRight=300$\r$\nBottom=$R8$\r$\n$\r$\n'
  ${EndIf}

  IntOp $R6 $R6 + 1
  IntOp $R7 $R7 + 12

  IntOp $R0 $NewComponents & ${ComponentDirector}
  ${If} $R0 <> 0
    ${If} "$ConfigDirectorName" == ""
      StrCpy $ConfigDirectorName "$HostName-dir"
    ${EndIf}
    ${If} "$ConfigDirectorPassword" == ""
      StrCpy $ConfigDirectorPassword "$LocalDirectorPassword"
    ${EndIf}
  ${Else}
    ${If} "$ConfigDirectorName" == "$HostName-dir"
      StrCpy $ConfigDirectorName ""
    ${EndIf}
    ${If} "$ConfigDirectorPassword" == "$LocalDirectorPassword"
      StrCpy $ConfigDirectorPassword ""
    ${EndIf}
  ${EndIf}

  IntOp $R0 $NewComponents & ${ComponentDirector}
  ${If} $R0 = 0
  ${OrIf} $AutomaticInstall = 0
    IntOp $R8 $R7 + 8
    FileWrite $R5 '[Field $R6]$\r$\nType="Label"$\r$\nText="DIR Name"$\r$\nLeft=6$\r$\nTop=$R7$\r$\nRight=60$\r$\nBottom=$R8$\r$\n$\r$\n'
    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 - 2

    IntOp $R8 $R8 + 2
    FileWrite $R5 '[Field $R6]$\r$\nType="Text"$\r$\nState=$ConfigDirectorName$\r$\nLeft=60$\r$\nTop=$R7$\r$\nRight=158$\r$\nBottom=$R8$\r$\n$\r$\n'
    IntOp $R6 $R6 + 1

    ${If} $AutomaticInstall = 0
      IntOp $R0 $NewComponents & ${ComponentsDirectorAndTextGuiConsoles}
      ${If} $R0 <> 0
        IntOp $R7 $R7 + 2
        IntOp $R8 $R8 - 2
        FileWrite $R5 '[Field $R6]$\r$\nType="Label"$\r$\nText="DIR Port"$\r$\nLeft=172$\r$\nTop=$R7$\r$\nRight=188$\r$\nBottom=$R8$\r$\n$\r$\n'
        IntOp $R6 $R6 + 1
        IntOp $R7 $R7 - 2

        IntOp $R8 $R8 + 2
        FileWrite $R5 '[Field $R6]$\r$\nType="Text"$\r$\nFlags="ONLY_NUMBERS"$\r$\nState=$ConfigDirectorPort$\r$\nLeft=190$\r$\nTop=$R7$\r$\nRight=218$\r$\nBottom=$R8$\r$\n$\r$\n'
        IntOp $R6 $R6 + 1
      ${EndIf}

      IntOp $R0 $NewComponents & ${ComponentDirector}
      ${If} $R0 <> 0
        IntOp $R7 $R7 + 2
        IntOp $R8 $R8 - 2
        FileWrite $R5 '[Field $R6]$\r$\nType="Label"$\r$\nText="Max Jobs"$\r$\nLeft=238$\r$\nTop=$R7$\r$\nRight=270$\r$\nBottom=$R8$\r$\n$\r$\n'
        IntOp $R6 $R6 + 1
        IntOp $R7 $R7 - 2

        IntOp $R8 $R8 + 2
        FileWrite $R5 '[Field $R6]$\r$\nType="Text"$\r$\nFlags="ONLY_NUMBERS"$\r$\nState=$ConfigDirectorMaxJobs$\r$\nLeft=274$\r$\nTop=$R7$\r$\nRight=294$\r$\nBottom=$R8$\r$\n$\r$\n'
        IntOp $R6 $R6 + 1
      ${EndIf}
    ${EndIf}

    IntOp $R7 $R7 + 14
  ${EndIf}

  IntOp $R0 $NewComponents & ${ComponentsTextAndGuiConsoles}
  ${If} $R0 <> 0
  ${OrIf} $AutomaticInstall = 0
    IntOp $R0 $NewComponents & ${ComponentsDirectorAndTextGuiConsoles}
    ${If} $R0 <> 0
      IntOp $R7 $R7 + 2
      IntOp $R8 $R7 + 8

      FileWrite $R5 '[Field $R6]$\r$\nType="Label"$\r$\nText="DIR Password"$\r$\nLeft=6$\r$\nTop=$R7$\r$\nRight=60$\r$\nBottom=$R8$\r$\n$\r$\n'

      IntOp $R6 $R6 + 1
      IntOp $R7 $R7 - 2
      IntOp $R8 $R8 + 2

      FileWrite $R5 '[Field $R6]$\r$\nType="Text"$\r$\nState=$ConfigDirectorPassword$\r$\nLeft=60$\r$\nTop=$R7$\r$\nRight=294$\r$\nBottom=$R8$\r$\n$\r$\n'

      IntOp $R6 $R6 + 1
      IntOp $R7 $R7 + 14
    ${EndIf}
  ${EndIf}

  IntOp $R0 $NewComponents & ${ComponentDirector}
  ${If} $R0 <> 0
    IntOp $R7 $R7 + 2
    IntOp $R8 $R7 + 8

    FileWrite $R5 '[Field $R6]$\r$\nType="Label"$\r$\nText="Mail Server"$\r$\nLeft=6$\r$\nTop=$R7$\r$\nRight=48$\r$\nBottom=$R8$\r$\n$\r$\n'

    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 - 2
    IntOp $R8 $R8 + 2

    FileWrite $R5 '[Field $R6]$\r$\nType="Text"$\r$\nState=$ConfigDirectorMailServer$\r$\nLeft=50$\r$\nTop=$R7$\r$\nRight=294$\r$\nBottom=$R8$\r$\n$\r$\n'
    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 + 16
    IntOp $R8 $R7 + 8

    FileWrite $R5 '[Field $R6]$\r$\nType="Label"$\r$\nText="Mail Address"$\r$\nLeft=6$\r$\nTop=$R7$\r$\nRight=48$\r$\nBottom=$R8$\r$\n$\r$\n'

    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 - 2
    IntOp $R8 $R8 + 2

    FileWrite $R5 '[Field $R6]$\r$\nType="Text"$\r$\nState=$ConfigDirectorMailAddress$\r$\nLeft=50$\r$\nTop=$R7$\r$\nRight=294$\r$\nBottom=$R8$\r$\n$\r$\n'
    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 + 16
    IntOp $R8 $R7 + 8

    FileWrite $R5 '[Field $R6]$\r$\nType="Label"$\r$\nText="Database"$\r$\nLeft=6$\r$\nTop=$R7$\r$\nRight=38$\r$\nBottom=$R8$\r$\n$\r$\n'

    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 - 2
    IntOp $R8 $R8 + 2

    ${If} $ConfigDirectorDB = 0
      ${If} $MySQLPath != ""
        StrCpy $ConfigDirectorDB 1
      ${ElseIf} $PostgreSQLPath != ""
        StrCpy $ConfigDirectorDB 2
      ${Else}
        StrCpy $ConfigDirectorDB 3
      ${EndIf}
    ${EndIf}

    ${If} $ConfigDirectorDB = 1
      StrCpy $R9 1
    ${Else}
      StrCpy $R9 0
    ${EndIf}

    FileWrite $R5 '[Field $R6]$\r$\nType="RadioButton"$\r$\nState=$R9$\r$\nText="MySQL"$\r$\nFlags="GROUP"$\r$\nLeft=50$\r$\nTop=$R7$\r$\nRight=90$\r$\nBottom=$R8$\r$\n$\r$\n'

    IntOp $R6 $R6 + 1

    ${If} $ConfigDirectorDB = 2
      StrCpy $R9 1
    ${Else}
      StrCpy $R9 0
    ${EndIf}

    FileWrite $R5 '[Field $R6]$\r$\nType="RadioButton"$\r$\nState=$R9$\r$\nText="PostgreSQL"$\r$\nFlags="NOTABSTOP"$\r$\nLeft=94$\r$\nTop=$R7$\r$\nRight=146$\r$\nBottom=$R8$\r$\n$\r$\n'

    IntOp $R6 $R6 + 1

    ${If} $ConfigDirectorDB = 3
      StrCpy $R9 1
    ${Else}
      StrCpy $R9 0
    ${EndIf}

    FileWrite $R5 '[Field $R6]$\r$\nType="RadioButton"$\r$\nState=$R9$\r$\nText="Sqlite"$\r$\nFlags="NOTABSTOP"$\r$\nLeft=150$\r$\nTop=$R7$\r$\nRight=182$\r$\nBottom=$R8$\r$\n$\r$\n'

    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 + 12

    ${If} $AutomaticInstall = 0
      IntOp $R8 $R7 + 10
      FileWrite $R5 '[Field $R6]$\r$\nType="Checkbox"$\r$\nState=$ConfigDirectorInstallService$\r$\nText="Install as service"$\r$\nLeft=50$\r$\nTop=$R7$\r$\nRight=118$\r$\nBottom=$R8$\r$\n$\r$\n'
      IntOp $R6 $R6 + 1

      FileWrite $R5 '[Field $R6]$\r$\nType="Checkbox"$\r$\nState=$ConfigDirectorStartService$\r$\nText="Start after install"$\r$\nLeft=190$\r$\nTop=$R7$\r$\nRight=260$\r$\nBottom=$R8$\r$\n$\r$\n'

      IntOp $R6 $R6 + 1
      IntOp $R7 $R7 + 12
    ${EndIf}
  ${Else}
    IntOp $R0 $NewComponents & ${ComponentsTextAndGuiConsoles}
    ${If} $R0 <> 0
      IntOp $R7 $R7 + 2
      IntOp $R8 $R7 + 8

      FileWrite $R5 '[Field $R6]$\r$\nType="Label"$\r$\nText="DIR Address"$\r$\nLeft=6$\r$\nTop=$R7$\r$\nRight=60$\r$\nBottom=$R8$\r$\n$\r$\n'

      IntOp $R6 $R6 + 1
      IntOp $R7 $R7 - 2
      IntOp $R8 $R8 + 2

      FileWrite $R5 '[Field $R6]$\r$\nType="Text"$\r$\nState=$ConfigDirectorAddress$\r$\nLeft=60$\r$\nTop=$R7$\r$\nRight=294$\r$\nBottom=$R8$\r$\n$\r$\n'
      IntOp $R6 $R6 + 1
      IntOp $R7 $R7 + 14
      IntOp $R8 $R7 + 8
    ${EndIf}
  ${EndIf}

  IntOp $R7 $R7 + 4

  ${If} $AutomaticInstall = 0
    IntOp $R0 $NewComponents & ${ComponentsFileAndStorageAndDirector}
    IntOp $R0 0 & 0
    ${If} $R0 <> 0
      IntOp $R8 $R7 + 42

      FileWrite $R5 '[Field $R6]$\r$\nType="GroupBox"$\r$\nText="Monitor"$\r$\nLeft=0$\r$\nTop=$R7$\r$\nRight=300$\r$\nBottom=$R8$\r$\n$\r$\n'
      IntOp $R6 $R6 + 1
      IntOp $R7 $R7 + 12

      IntOp $R8 $R7 + 8
      FileWrite $R5 '[Field $R6]$\r$\nType="Label"$\r$\nText="Name"$\r$\nLeft=6$\r$\nTop=$R7$\r$\nRight=26$\r$\nBottom=$R8$\r$\n$\r$\n'
      IntOp $R6 $R6 + 1
      IntOp $R7 $R7 - 2

      IntOp $R8 $R8 + 2
      FileWrite $R5 '[Field $R6]$\r$\nType="Text"$\r$\nState=$ConfigMonitorName$\r$\nLeft=50$\r$\nTop=$R7$\r$\nRight=150$\r$\nBottom=$R8$\r$\n$\r$\n'
      IntOp $R6 $R6 + 1
      IntOp $R7 $R7 + 16
      IntOp $R8 $R7 + 8

      FileWrite $R5 '[Field $R6]$\r$\nType="Label"$\r$\nText="Password"$\r$\nLeft=6$\r$\nTop=$R7$\r$\nRight=38$\r$\nBottom=$R8$\r$\n$\r$\n'

      IntOp $R6 $R6 + 1
      IntOp $R7 $R7 - 2
      IntOp $R8 $R8 + 2

      FileWrite $R5 '[Field $R6]$\r$\nType="Text"$\r$\nState=$ConfigMonitorPassword$\r$\nLeft=50$\r$\nTop=$R7$\r$\nRight=294$\r$\nBottom=$R8$\r$\n$\r$\n'

      IntOp $R6 $R6 + 1
      IntOp $R7 $R7 + 20
    ${EndIf}
  ${EndIf}

  IntOp $R6 $R6 - 1
  FileWrite $R5 "[Settings]$\r$\nNumFields=$R6$\r$\n"

  FileClose $R5

  IntOp $R0 $NewComponents & ${ComponentsFileAndStorage}
  ${If} $R0 = 0
  ${OrIf} $AutomaticInstall = 1
    !insertmacro MUI_HEADER_TEXT "$(TITLE_ConfigPage1)" "$(SUBTITLE_ConfigPage1)"
  ${Else}
    !insertmacro MUI_HEADER_TEXT "$(TITLE_ConfigPage2)" "$(SUBTITLE_ConfigPage2)"
  ${EndIf}

  !insertmacro MUI_INSTALLOPTIONS_INITDIALOG "ConfigPage2.ini"
  Pop $HDLG ;HWND of dialog

  ; Initialize Controls
  StrCpy $R6 2  ; Field Number

  IntOp $R0 $NewComponents & ${ComponentDirector}
  ${If} $R0 = 0
  ${OrIf} $AutomaticInstall = 0
    ; Name
    IntOp $R6 $R6 + 1
    !insertmacro MUI_INSTALLOPTIONS_READ $HCTL "ConfigPage2.ini" "Field $R6" "HWND"
    SendMessage $HCTL ${EM_LIMITTEXT} 30 0
    IntOp $R6 $R6 + 1

    ${If} $AutomaticInstall = 0
      IntOp $R0 $NewComponents & ${ComponentsDirectorAndTextGuiConsoles}
      ${If} $R0 <> 0
        IntOp $R6 $R6 + 1
        ; Port Number
        !insertmacro MUI_INSTALLOPTIONS_READ $HCTL "ConfigPage2.ini" "Field $R6" "HWND"
        SendMessage $HCTL ${EM_LIMITTEXT} 5 0
        IntOp $R6 $R6 + 1
      ${EndIf}

      IntOp $R0 $NewComponents & ${ComponentDirector}
      ${If} $R0 <> 0
        IntOp $R6 $R6 + 1
        ; Max Jobs
        !insertmacro MUI_INSTALLOPTIONS_READ $HCTL "ConfigPage2.ini" "Field $R6" "HWND"
        SendMessage $HCTL ${EM_LIMITTEXT} 3 0

        IntOp $R6 $R6 + 1
      ${EndIf}
    ${EndIf}
  ${EndIf}

  IntOp $R0 $NewComponents & ${ComponentsTextAndGuiConsoles}
  ${If} $R0 <> 0
  ${OrIf} $AutomaticInstall = 0
    IntOp $R0 $NewComponents & ${ComponentsDirectorAndTextGuiConsoles}
    ${If} $R0 <> 0
      IntOp $R6 $R6 + 2
    ${EndIf}
  ${EndIf}

  IntOp $R0 $NewComponents & ${ComponentDirector}
  ${If} $R0 <> 0
    IntOp $R6 $R6 + 9

    ${If} $AutomaticInstall = 0
      IntOp $R6 $R6 + 2
    ${EndIf}
  ${Else}
    IntOp $R0 $NewComponents & ${ComponentsTextAndGuiConsoles}
    ${If} $R0 <> 0
      IntOp $R6 $R6 + 2
    ${EndIf}
  ${EndIf}

  ${If} $AutomaticInstall = 0
    IntOp $R0 $NewComponents & ${ComponentsFileAndStorageAndDirector}
    ${If} $R0 <> 0
      IntOp $R6 $R6 + 2
      !insertmacro MUI_INSTALLOPTIONS_READ $HCTL "ConfigPage2.ini" "Field $R6" "HWND"
      SendMessage $HCTL ${EM_LIMITTEXT} 30 0
      IntOp $R6 $R6 + 2
    ${EndIf}
  ${EndIf}

  !insertmacro MUI_INSTALLOPTIONS_SHOW

  ; Process results

  StrCpy $R6 2

  IntOp $R0 $NewComponents & ${ComponentDirector}
  ${If} $R0 = 0
  ${OrIf} $AutomaticInstall = 0
    IntOp $R6 $R6 + 1
    !insertmacro MUI_INSTALLOPTIONS_READ $ConfigDirectorName "ConfigPage2.ini" "Field $R6" "State"
    IntOp $R6 $R6 + 1

    ${If} $AutomaticInstall = 0
      IntOp $R0 $NewComponents & ${ComponentsDirectorAndTextGuiConsoles}
      ${If} $R0 <> 0
        IntOp $R6 $R6 + 1
        !insertmacro MUI_INSTALLOPTIONS_READ $ConfigDirectorPort "ConfigPage2.ini" "Field $R6" "State"
        IntOp $R6 $R6 + 1
      ${EndIf}

      IntOp $R0 $NewComponents & ${ComponentDirector}
      ${If} $R0 <> 0
        IntOp $R6 $R6 + 1
        !insertmacro MUI_INSTALLOPTIONS_READ $ConfigDirectorMaxJobs "ConfigPage2.ini" "Field $R6" "State"
        IntOp $R6 $R6 + 1
      ${EndIf}
    ${EndIf}
  ${EndIf}

  IntOp $R0 $NewComponents & ${ComponentsTextAndGuiConsoles}
  ${If} $R0 <> 0
  ${OrIf} $AutomaticInstall = 0
    IntOp $R0 $NewComponents & ${ComponentsDirectorAndTextGuiConsoles}
    ${If} $R0 <> 0
      IntOp $R6 $R6 + 1
      !insertmacro MUI_INSTALLOPTIONS_READ $ConfigDirectorPassword "ConfigPage2.ini" "Field $R6" "State"
      IntOp $R6 $R6 + 1
    ${EndIf}
  ${EndIf}

  IntOp $R0 $NewComponents & ${ComponentDirector}
  ${If} $R0 <> 0
    IntOp $R6 $R6 + 1
    !insertmacro MUI_INSTALLOPTIONS_READ $ConfigDirectorMailServer "ConfigPage2.ini" "Field $R6" "State"
    IntOp $R6 $R6 + 2
    !insertmacro MUI_INSTALLOPTIONS_READ $ConfigDirectorMailAddress "ConfigPage2.ini" "Field $R6" "State"
    IntOp $R6 $R6 + 2
    !insertmacro MUI_INSTALLOPTIONS_READ $R5 "ConfigPage2.ini" "Field $R6" "State"
    ${If} $R5 = 1
      StrCpy $ConfigDirectorDB 1
    ${Endif}
    IntOp $R6 $R6 + 1
    !insertmacro MUI_INSTALLOPTIONS_READ $R5 "ConfigPage2.ini" "Field $R6" "State"
    ${If} $R5 = 1
      StrCpy $ConfigDirectorDB 2
    ${Endif}
    IntOp $R6 $R6 + 1
    !insertmacro MUI_INSTALLOPTIONS_READ $R5 "ConfigPage2.ini" "Field $R6" "State"
    ${If} $R5 = 1
      StrCpy $ConfigDirectorDB 3
    ${Endif}
    IntOp $R6 $R6 + 1

    ${If} $AutomaticInstall = 0
      !insertmacro MUI_INSTALLOPTIONS_READ $ConfigDirectorInstallService "ConfigPage2.ini" "Field $R6" "State"
      IntOp $R6 $R6 + 1
      !insertmacro MUI_INSTALLOPTIONS_READ $ConfigDirectorStartService "ConfigPage2.ini" "Field $R6" "State"
      IntOp $R6 $R6 + 1
    ${EndIf}
  ${Else}
    IntOp $R0 $NewComponents & ${ComponentsTextAndGuiConsoles}
    ${If} $R0 <> 0
      IntOp $R6 $R6 + 1
      !insertmacro MUI_INSTALLOPTIONS_READ $ConfigDirectorAddress "ConfigPage2.ini" "Field $R6" "State"
      IntOp $R6 $R6 + 1
    ${EndIf}
  ${EndIf}

  ${If} $AutomaticInstall = 0
    IntOp $R0 $NewComponents & ${ComponentsFileAndStorageAndDirector}
    ${If} $R0 <> 0
      IntOp $R6 $R6 + 2
      !insertmacro MUI_INSTALLOPTIONS_READ $ConfigMonitorName "ConfigPage2.ini" "Field $R6" "State"
      IntOp $R6 $R6 + 2
      !insertmacro MUI_INSTALLOPTIONS_READ $ConfigMonitorPassword "ConfigPage2.ini" "Field $R6" "State"
    ${EndIf}
  ${EndIf}
FunctionEnd

Function LeaveConfigPage2
  ${If} $AutomaticInstall = 0
    StrCpy $R6 4

    IntOp $R0 $NewComponents & ${ComponentsDirectorAndTextGuiConsoles}
    ${If} $R0 <> 0
      IntOp $R6 $R6 + 1
      !insertmacro MUI_INSTALLOPTIONS_READ $R0 "ConfigPage2.ini" "Field $R6" "State"
      ${If} $R0 < 1024
      ${OrIf} $R0 > 65535
        MessageBox MB_OK "Port must be between 1024 and 65535 inclusive."
        Abort
      ${EndIf}
      IntOp $R6 $R6 + 1
    ${EndIf}

    IntOp $R0 $NewComponents & ${ComponentDirector}
    ${If} $R0 <> 0
      IntOp $R6 $R6 + 1
      !insertmacro MUI_INSTALLOPTIONS_READ $R0 "ConfigPage2.ini" "Field $R6" "State"
      ${If} $R0 < 1
      ${OrIf} $R0 > 99
        MessageBox MB_OK "Max Jobs must be between 1 and 99 inclusive."
        Abort
      ${EndIf}
      IntOp $R6 $R6 + 1
    ${EndIf}
  ${EndIf}
FunctionEnd
