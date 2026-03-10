; EasyShotter NSIS Installer Script
; Version: 0.2.0

!include "MUI2.nsh"

; General
Name "EasyShotter"
OutFile "..\installer\EasyShotter-0.2.0-Setup.exe"
InstallDir "$PROGRAMFILES64\EasyShotter"
InstallDirRegKey HKLM "Software\EasyShotter" "InstallDir"
RequestExecutionLevel admin

; Version info
VIProductVersion "0.2.0.0"
VIAddVersionKey "ProductName" "EasyShotter"
VIAddVersionKey "ProductVersion" "0.2.0"
VIAddVersionKey "FileDescription" "EasyShotter Screenshot Tool Installer"
VIAddVersionKey "FileVersion" "0.2.0"

; Icon
!define MUI_ICON "..\resources\app_icon.ico"
!define MUI_UNICON "..\resources\app_icon.ico"

; Interface Settings
!define MUI_ABORTWARNING

; Pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN "$INSTDIR\EasyShotter.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Launch EasyShotter"
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; Language
!insertmacro MUI_LANGUAGE "SimpChinese"
!insertmacro MUI_LANGUAGE "English"

; Installer Section
Section "Install"
    SetOutPath "$INSTDIR"

    ; Main executable and DLLs
    File /r "..\deploy\*.*"

    ; Create uninstaller
    WriteUninstaller "$INSTDIR\Uninstall.exe"

    ; Registry entries
    WriteRegStr HKLM "Software\EasyShotter" "InstallDir" "$INSTDIR"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\EasyShotter" \
        "DisplayName" "EasyShotter"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\EasyShotter" \
        "DisplayVersion" "0.2.0"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\EasyShotter" \
        "UninstallString" "$\"$INSTDIR\Uninstall.exe$\""
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\EasyShotter" \
        "DisplayIcon" "$INSTDIR\app_icon.ico"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\EasyShotter" \
        "Publisher" "EasyShotter"
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\EasyShotter" \
        "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\EasyShotter" \
        "NoRepair" 1

    ; Start menu shortcut
    CreateDirectory "$SMPROGRAMS\EasyShotter"
    CreateShortcut "$SMPROGRAMS\EasyShotter\EasyShotter.lnk" "$INSTDIR\EasyShotter.exe" \
        "" "$INSTDIR\app_icon.ico"
    CreateShortcut "$SMPROGRAMS\EasyShotter\Uninstall.lnk" "$INSTDIR\Uninstall.exe"

    ; Desktop shortcut
    CreateShortcut "$DESKTOP\EasyShotter.lnk" "$INSTDIR\EasyShotter.exe" \
        "" "$INSTDIR\app_icon.ico"
SectionEnd

; Uninstaller Section
Section "Uninstall"
    ; Kill running process
    nsExec::ExecToLog 'taskkill /F /IM EasyShotter.exe'

    ; Remove files
    RMDir /r "$INSTDIR"

    ; Remove shortcuts
    Delete "$SMPROGRAMS\EasyShotter\EasyShotter.lnk"
    Delete "$SMPROGRAMS\EasyShotter\Uninstall.lnk"
    RMDir "$SMPROGRAMS\EasyShotter"
    Delete "$DESKTOP\EasyShotter.lnk"

    ; Remove registry entries
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\EasyShotter"
    DeleteRegKey HKLM "Software\EasyShotter"
SectionEnd
