Function EnterConfigPage1
  ${If} $AutomaticInstall = 1
    Abort
  ${EndIf}

  IntOp $R0 $NewComponents & ${ComponentsFileAndStorage}

  ${If} $R0 = 0
    Abort
  ${EndIf}

  FileOpen $R5 "$PLUGINSDIR\ConfigPage1.ini" w

  StrCpy $R6 1  ; Field Number
  StrCpy $R7 0  ; Top

  IntOp $R0 $NewComponents & ${ComponentFile}
  ${If} $R0 <> 0
    IntOp $R8 $R7 + 52
    FileWrite $R5 '[Field $R6]$\r$\nType="GroupBox"$\r$\nText="Client"$\r$\nLeft=0$\r$\nTop=$R7$\r$\nRight=300$\r$\nBottom=$R8$\r$\n$\r$\n'
    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 + 12

    IntOp $R8 $R7 + 8
    FileWrite $R5 '[Field $R6]$\r$\nType="Label"$\r$\nText="Name"$\r$\nLeft=6$\r$\nTop=$R7$\r$\nRight=26$\r$\nBottom=$R8$\r$\n$\r$\n'
    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 - 2

    IntOp $R8 $R8 + 2
    FileWrite $R5 '[Field $R6]$\r$\nType="Text"$\r$\nState=$ConfigClientName$\r$\nLeft=50$\r$\nTop=$R7$\r$\nRight=158$\r$\nBottom=$R8$\r$\n$\r$\n'
    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 + 2

    IntOp $R8 $R8 - 2
    FileWrite $R5 '[Field $R6]$\r$\nType="Label"$\r$\nText="Port"$\r$\nLeft=172$\r$\nTop=$R7$\r$\nRight=188$\r$\nBottom=$R8$\r$\n$\r$\n'
    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 - 2

    IntOp $R8 $R8 + 2
    FileWrite $R5 '[Field $R6]$\r$\nType="Text"$\r$\nFlags="ONLY_NUMBERS"$\r$\nState=$ConfigClientPort$\r$\nLeft=190$\r$\nTop=$R7$\r$\nRight=218$\r$\nBottom=$R8$\r$\n$\r$\n'
    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 + 2

    IntOp $R8 $R8 - 2
    FileWrite $R5 '[Field $R6]$\r$\nType="Label"$\r$\nText="Max Jobs"$\r$\nLeft=238$\r$\nTop=$R7$\r$\nRight=270$\r$\nBottom=$R8$\r$\n$\r$\n'
    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 - 2

    IntOp $R8 $R8 + 2
    FileWrite $R5 '[Field $R6]$\r$\nType="Text"$\r$\nFlags="ONLY_NUMBERS"$\r$\nState=$ConfigClientMaxJobs$\r$\nLeft=274$\r$\nTop=$R7$\r$\nRight=294$\r$\nBottom=$R8$\r$\n$\r$\n'
    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 + 16

    IntOp $R8 $R7 + 8
    FileWrite $R5 '[Field $R6]$\r$\nType="Label"$\r$\nText="Password"$\r$\nLeft=6$\r$\nTop=$R7$\r$\nRight=38$\r$\nBottom=$R8$\r$\n$\r$\n'
    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 - 2

    IntOp $R8 $R8 + 2
    FileWrite $R5 '[Field $R6]$\r$\nType="Text"$\r$\nState=$ConfigClientPassword$\r$\nLeft=50$\r$\nTop=$R7$\r$\nRight=294$\r$\nBottom=$R8$\r$\n$\r$\n'
    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 + 14

    IntOp $R8 $R7 + 10
    FileWrite $R5 '[Field $R6]$\r$\nType="Checkbox"$\r$\nState=$ConfigClientInstallService$\r$\nText="Install as service"$\r$\nLeft=50$\r$\nTop=$R7$\r$\nRight=118$\r$\nBottom=$R8$\r$\n$\r$\n'
    IntOp $R6 $R6 + 1

    FileWrite $R5 '[Field $R6]$\r$\nType="Checkbox"$\r$\nState=$ConfigClientStartService$\r$\nText="Start after install"$\r$\nLeft=190$\r$\nTop=$R7$\r$\nRight=260$\r$\nBottom=$R8$\r$\n$\r$\n'
    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 + 16
  ${Endif}

  IntOp $R0 $NewComponents & ${ComponentStorage}
  ${If} $R0 <> 0
    IntOp $R8 $R7 + 52
    FileWrite $R5 '[Field $R6]$\r$\nType="GroupBox"$\r$\nText="Storage"$\r$\nLeft=0$\r$\nTop=$R7$\r$\nRight=300$\r$\nBottom=$R8$\r$\n$\r$\n'
    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 + 12
    
    IntOp $R8 $R7 + 8
    FileWrite $R5 '[Field $R6]$\r$\nType="Label"$\r$\nText="Name"$\r$\nLeft=6$\r$\nTop=$R7$\r$\nRight=26$\r$\nBottom=$R8$\r$\n$\r$\n'
    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 - 2

    IntOp $R8 $R8 + 2
    FileWrite $R5 '[Field $R6]$\r$\nType="Text"$\r$\nState=$ConfigStorageName$\r$\nLeft=50$\r$\nTop=$R7$\r$\nRight=158$\r$\nBottom=$R8$\r$\n$\r$\n'
    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 + 2

    IntOp $R8 $R8 - 2
    FileWrite $R5 '[Field $R6]$\r$\nType="Label"$\r$\nText="Port"$\r$\nLeft=172$\r$\nTop=$R7$\r$\nRight=188$\r$\nBottom=$R8$\r$\n$\r$\n'
    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 - 2

    IntOp $R8 $R8 + 2
    FileWrite $R5 '[Field $R6]$\r$\nType="Text"$\r$\nFlags="ONLY_NUMBERS"$\r$\nState=$ConfigStoragePort$\r$\nLeft=190$\r$\nTop=$R7$\r$\nRight=218$\r$\nBottom=$R8$\r$\n$\r$\n'
    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 + 2

    IntOp $R8 $R8 - 2
    FileWrite $R5 '[Field $R6]$\r$\nType="Label"$\r$\nText="Max Jobs"$\r$\nLeft=238$\r$\nTop=$R7$\r$\nRight=270$\r$\nBottom=$R8$\r$\n$\r$\n'
    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 - 2

    IntOp $R8 $R8 + 2
    FileWrite $R5 '[Field $R6]$\r$\nType="Text"$\r$\nFlags="ONLY_NUMBERS"$\r$\nState=$ConfigStorageMaxJobs$\r$\nLeft=274$\r$\nTop=$R7$\r$\nRight=294$\r$\nBottom=$R8$\r$\n$\r$\n'
    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 + 16

    IntOp $R8 $R7 + 8
    FileWrite $R5 '[Field $R6]$\r$\nType="Label"$\r$\nText="Password"$\r$\nLeft=6$\r$\nTop=$R7$\r$\nRight=38$\r$\nBottom=$R8$\r$\n$\r$\n'
    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 - 2

    IntOp $R8 $R8 + 2
    FileWrite $R5 '[Field $R6]$\r$\nType="Text"$\r$\nState=$ConfigStoragePassword$\r$\nLeft=50$\r$\nTop=$R7$\r$\nRight=294$\r$\nBottom=$R8$\r$\n$\r$\n'
    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 + 14

    IntOp $R8 $R7 + 10
    FileWrite $R5 '[Field $R6]$\r$\nType="Checkbox"$\r$\nState=$ConfigStorageInstallService$\r$\nText="Install as service"$\r$\nLeft=50$\r$\nTop=$R7$\r$\nRight=118$\r$\nBottom=$R8$\r$\n$\r$\n'
    IntOp $R6 $R6 + 1

    FileWrite $R5 '[Field $R6]$\r$\nType="Checkbox"$\r$\nState=$ConfigStorageStartService$\r$\nText="Start after install"$\r$\nLeft=190$\r$\nTop=$R7$\r$\nRight=260$\r$\nBottom=$R8$\r$\n$\r$\n'
    IntOp $R6 $R6 + 1
    IntOp $R7 $R7 + 16
  ${Endif}

  IntOp $R6 $R6 - 1

  FileWrite $R5 "[Settings]$\r$\nNumFields=$R6$\r$\n"

  FileClose $R5

  !insertmacro MUI_HEADER_TEXT "$(TITLE_ConfigPage1)" "$(SUBTITLE_ConfigPage1)"
  !insertmacro MUI_INSTALLOPTIONS_INITDIALOG "ConfigPage1.ini"
  Pop $HDLG ;HWND of dialog

  ; Initialize Controls

  StrCpy $R6 1  ; Field Number

  IntOp $R0 $NewComponents & ${ComponentFile}
  ${If} $R0 <> 0
    IntOp $R6 $R6 + 2

    ; Client Name
    !insertmacro MUI_INSTALLOPTIONS_READ $HCTL "ConfigPage1.ini" "Field $R6" "HWND"
    SendMessage $HCTL ${EM_LIMITTEXT} 30 0

    IntOp $R6 $R6 + 2

    ; Client Port Number
    !insertmacro MUI_INSTALLOPTIONS_READ $HCTL "ConfigPage1.ini" "Field $R6" "HWND"
    SendMessage $HCTL ${EM_LIMITTEXT} 5 0

    IntOp $R6 $R6 + 2

    ; Max Jobs
    !insertmacro MUI_INSTALLOPTIONS_READ $HCTL "ConfigPage1.ini" "Field $R6" "HWND"
    SendMessage $HCTL ${EM_LIMITTEXT} 3 0

    IntOp $R6 $R6 + 5
  ${Endif}

  IntOp $R0 $NewComponents & ${ComponentStorage}
  ${If} $R0 <> 0
    IntOp $R6 $R6 + 2

    ; Storage Name
    !insertmacro MUI_INSTALLOPTIONS_READ $HCTL "ConfigPage1.ini" "Field $R6" "HWND"
    SendMessage $HCTL ${EM_LIMITTEXT} 30 0

    IntOp $R6 $R6 + 2

    ; Storage Port Number
    !insertmacro MUI_INSTALLOPTIONS_READ $HCTL "ConfigPage1.ini" "Field $R6" "HWND"
    SendMessage $HCTL ${EM_LIMITTEXT} 5 0

    IntOp $R6 $R6 + 2

    ; Max Jobs
    !insertmacro MUI_INSTALLOPTIONS_READ $HCTL "ConfigPage1.ini" "Field $R6" "HWND"
    SendMessage $HCTL ${EM_LIMITTEXT} 3 0

    IntOp $R6 $R6 + 5
  ${Endif}

  !insertmacro MUI_INSTALLOPTIONS_SHOW

  ; Process results

  StrCpy $R6 3

  IntOp $R0 $NewComponents & ${ComponentFile}
  ${If} $R0 <> 0
    !insertmacro MUI_INSTALLOPTIONS_READ $ConfigClientName "ConfigPage1.ini" "Field $R6" "State"

    IntOp $R6 $R6 + 2

    !insertmacro MUI_INSTALLOPTIONS_READ $ConfigClientPort "ConfigPage1.ini" "Field $R6" "State"

    IntOp $R6 $R6 + 2

    !insertmacro MUI_INSTALLOPTIONS_READ $ConfigClientMaxJobs "ConfigPage1.ini" "Field $R6" "State"

    IntOp $R6 $R6 + 2

    !insertmacro MUI_INSTALLOPTIONS_READ $ConfigClientPassword "ConfigPage1.ini" "Field $R6" "State"

    IntOp $R6 $R6 + 1

    !insertmacro MUI_INSTALLOPTIONS_READ $ConfigClientInstallService "ConfigPage1.ini" "Field $R6" "State"

    IntOp $R6 $R6 + 1

    !insertmacro MUI_INSTALLOPTIONS_READ $ConfigClientStartService "ConfigPage1.ini" "Field $R6" "State"

    IntOp $R6 $R6 + 3
  ${Endif}

  IntOp $R0 $NewComponents & ${ComponentStorage}
  ${If} $R0 <> 0
    !insertmacro MUI_INSTALLOPTIONS_READ $ConfigStorageName "ConfigPage1.ini" "Field $R6" "State"

    IntOp $R6 $R6 + 2

    !insertmacro MUI_INSTALLOPTIONS_READ $ConfigStoragePort "ConfigPage1.ini" "Field $R6" "State"

    IntOp $R6 $R6 + 2

    !insertmacro MUI_INSTALLOPTIONS_READ $ConfigStorageMaxJobs "ConfigPage1.ini" "Field $R6" "State"

    IntOp $R6 $R6 + 2

    !insertmacro MUI_INSTALLOPTIONS_READ $ConfigStoragePassword "ConfigPage1.ini" "Field $R6" "State"

    IntOp $R6 $R6 + 1

    !insertmacro MUI_INSTALLOPTIONS_READ $ConfigStorageInstallService "ConfigPage1.ini" "Field $R6" "State"

    IntOp $R6 $R6 + 1

    !insertmacro MUI_INSTALLOPTIONS_READ $ConfigStorageStartService "ConfigPage1.ini" "Field $R6" "State"

    IntOp $R6 $R6 + 3
  ${Endif}
FunctionEnd

Function LeaveConfigPage1
  StrCpy $R6 5

  IntOp $R0 $NewComponents & ${ComponentFile}
  ${If} $R0 <> 0
    !insertmacro MUI_INSTALLOPTIONS_READ $R0 "ConfigPage1.ini" "Field $R6" "State"
    ${If} $R0 < 1024
    ${OrIf} $R0 > 65535
      MessageBox MB_OK "Port must be between 1024 and 65535 inclusive."
      Abort
    ${EndIf}

    IntOp $R6 $R6 + 2

    !insertmacro MUI_INSTALLOPTIONS_READ $R0 "ConfigPage1.ini" "Field $R6" "State"
    ${If} $R0 < 1
    ${OrIf} $R0 > 99
      MessageBox MB_OK "Max Jobs must be between 1 and 99 inclusive."
      Abort
    ${EndIf}

    IntOp $R6 $R6 + 9
  ${Endif}
  
  IntOp $R0 $NewComponents & ${ComponentStorage}
  ${If} $R0 <> 0
    !insertmacro MUI_INSTALLOPTIONS_READ $R0 "ConfigPage1.ini" "Field $R6" "State"
    ${If} $R0 < 1024
    ${OrIf} $R0 > 65535
      MessageBox MB_OK "Port must be between 1024 and 65535 inclusive."
      Abort
    ${EndIf}

    IntOp $R6 $R6 + 2

    !insertmacro MUI_INSTALLOPTIONS_READ $R0 "ConfigPage1.ini" "Field $R6" "State"
    ${If} $R0 < 1
    ${OrIf} $R0 > 99
      MessageBox MB_OK "Max Jobs must be between 1 and 99 inclusive."
      Abort
    ${EndIf}

    IntOp $R6 $R6 + 9
  ${Endif}
FunctionEnd
