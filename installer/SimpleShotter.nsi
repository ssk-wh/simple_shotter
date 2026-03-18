; SimpleShotter NSIS Installer Script

!include "MUI2.nsh"
!include "FileFunc.nsh"

; ============== Basic Info ==============
!define APP_NAME "SimpleShotter"
!define APP_VERSION "0.2.0"
!define APP_PUBLISHER "SimpleShotter"
!define APP_EXE "SimpleShotter.exe"
!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}"

Name "${APP_NAME} ${APP_VERSION}"
OutFile "${APP_NAME}-${APP_VERSION}-Setup.exe"
InstallDir "$PROGRAMFILES64\${APP_NAME}"
InstallDirRegKey HKLM "${UNINSTALL_KEY}" "InstallLocation"
RequestExecutionLevel admin
Unicode True

; ============== MUI Settings ==============
!define MUI_ABORTWARNING
!define MUI_ICON "..\resources\app_icon.ico"
!define MUI_UNICON "..\resources\app_icon.ico"

; ============== Pages ==============
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES

!define MUI_FINISHPAGE_RUN "$INSTDIR\${APP_EXE}"
!define MUI_FINISHPAGE_RUN_TEXT "启动 ${APP_NAME}"
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; ============== Languages ==============
!insertmacro MUI_LANGUAGE "SimpChinese"

; ============== Installer Sections ==============
Section "主程序 (必需)" SecMain
    SectionIn RO

    SetOutPath "$INSTDIR"

    ; Main executable and DLLs
    File "dist\${APP_EXE}"
    File "dist\*.dll"
    File /nonfatal "dist\app_icon.ico"

    ; Qt plugins
    SetOutPath "$INSTDIR\platforms"
    File "dist\platforms\*.dll"
    SetOutPath "$INSTDIR\imageformats"
    File "dist\imageformats\*.dll"
    SetOutPath "$INSTDIR\iconengines"
    File "dist\iconengines\*.dll"
    SetOutPath "$INSTDIR\styles"
    File /nonfatal "dist\styles\*.dll"

    ; Create uninstaller
    SetOutPath "$INSTDIR"
    WriteUninstaller "$INSTDIR\Uninstall.exe"

    ; Write registry info for Add/Remove Programs
    WriteRegStr HKLM "${UNINSTALL_KEY}" "DisplayName" "${APP_NAME}"
    WriteRegStr HKLM "${UNINSTALL_KEY}" "DisplayVersion" "${APP_VERSION}"
    WriteRegStr HKLM "${UNINSTALL_KEY}" "Publisher" "${APP_PUBLISHER}"
    WriteRegStr HKLM "${UNINSTALL_KEY}" "UninstallString" "$\"$INSTDIR\Uninstall.exe$\""
    WriteRegStr HKLM "${UNINSTALL_KEY}" "InstallLocation" "$INSTDIR"
    WriteRegStr HKLM "${UNINSTALL_KEY}" "DisplayIcon" "$INSTDIR\${APP_EXE}"
    WriteRegDWORD HKLM "${UNINSTALL_KEY}" "NoModify" 1
    WriteRegDWORD HKLM "${UNINSTALL_KEY}" "NoRepair" 1

    ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
    WriteRegDWORD HKLM "${UNINSTALL_KEY}" "EstimatedSize" $0
SectionEnd

Section "开始菜单快捷方式" SecStartMenu
    CreateDirectory "$SMPROGRAMS\${APP_NAME}"
    CreateShortcut "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}"
    CreateShortcut "$SMPROGRAMS\${APP_NAME}\卸载 ${APP_NAME}.lnk" "$INSTDIR\Uninstall.exe"
SectionEnd

Section "桌面快捷方式" SecDesktop
    CreateShortcut "$DESKTOP\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}"
SectionEnd

; ============== Section Descriptions ==============
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecMain} "安装 SimpleShotter 主程序及所有必需文件"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecStartMenu} "在开始菜单创建快捷方式"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecDesktop} "在桌面创建快捷方式"
!insertmacro MUI_FUNCTION_DESCRIPTION_END

; ============== Uninstaller ==============
Section "Uninstall"
    ; Kill running process
    nsExec::ExecToLog 'taskkill /F /IM ${APP_EXE}'

    ; Remove uninstall registry key
    DeleteRegKey HKLM "${UNINSTALL_KEY}"

    ; Remove files
    Delete "$INSTDIR\${APP_EXE}"
    Delete "$INSTDIR\*.dll"
    Delete "$INSTDIR\*.ico"
    Delete "$INSTDIR\Uninstall.exe"

    RMDir /r "$INSTDIR\platforms"
    RMDir /r "$INSTDIR\imageformats"
    RMDir /r "$INSTDIR\iconengines"
    RMDir /r "$INSTDIR\styles"
    RMDir "$INSTDIR"

    ; Remove shortcuts
    Delete "$DESKTOP\${APP_NAME}.lnk"
    RMDir /r "$SMPROGRAMS\${APP_NAME}"
SectionEnd
