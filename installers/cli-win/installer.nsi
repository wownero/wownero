;-----------------------------------------------------------------------------------------
; File:     Wownero CLI NSIS installer
; Author:   0x000090
; Date:     1 Dec 2018
; License:  WTFPL
;-----------------------------------------------------------------------------------------
;
;                      DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
;                             Version 2, December 2004
;
;     Copyright (C) 2018  Wownero Inc., a Monero Enterprise Alliance partner company
;
;                      DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
;           TERMS AND CONDITIONS FOR COPYING, DISTRIBUTION AND MODIFICATION
;
;                       0. You just DO WHAT THE FUCK YOU WANT TO.
;
;-----------------------------------------------------------------------------------------
!include 'MUI2.nsh'
!include 'TextFunc.nsh'
!include 'WordFunc.nsh'
!include 'Sections.nsh'

;-----------------------------------------------------------------------------------------
!ifndef VERSION_MAJOR
    !warning 'VERSION_MAJOR not defined!'
    Quit
!endif

!ifndef VERSION_MINOR
    !warning 'VERSION_MINOR not defined!'
    Quit
!endif

!ifndef VERSION_BUILD
    !warning 'VERSION_BUILD not defined!'
    Quit
!endif

!ifndef VERSION_REVISION
    !warning 'VERSION_REVISION not defined!'
    Quit
!endif

!ifndef RELEASE_NAME
    !warning 'RELEASE_NAME not defined!'
    Quit
!endif

!define VERSION_SHORT                   '${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_BUILD}'
!define VERSION_LONG                    '${VERSION_SHORT}.${VERSION_REVISION}'

;-----------------------------------------------------------------------------------------
!define PROJECT_NAME                    'Wownero'
!define PRODUCT_NAME                    'Wownero CLI'
!define COMPANY_NAME                    'Wownero Inc.'
!define EXE_BASENAME                    'wownero'
!define HELP_URL                        'http://wownero.org/#community'
!define ABOUT_URL                       'http://wownero.org/#about'
!define SOURCE_DIR                      '..\..\build\release\bin'
!define TARGET_DIR                      '..\..\build\installers'
!define TARGET_PATH                     '${TARGET_DIR}\Wownero-CLI-${VERSION_LONG}-win.exe'
!define INSTALL_SUBDIR                  'cli'
!define INSTALL_SIZE                    181000
!define UNINSTALLER_NAME                'uninstall.exe'
!define REG_BASE                        'Software\Microsoft\Windows\CurrentVersion\Uninstall'
!define REG_KEY                         '${REG_BASE}\{E114584F-4E1B-4B2D-8B1C-69A188025E98}'
!define /date CURRENT_YEAR              '%Y'
  
;-----------------------------------------------------------------------------------------
!system 'if not exist "${TARGET_DIR}" md "${TARGET_DIR}"'

;-----------------------------------------------------------------------------------------
Name                                    '${PRODUCT_NAME}'
OutFile                                 '${TARGET_PATH}'
BrandingText                            '${PRODUCT_NAME} ${VERSION_LONG} "${RELEASE_NAME}"'
CRCCheck                                force
RequestExecutionLevel                   admin
InstallDir                              "$PROGRAMFILES64\${PROJECT_NAME}\${INSTALL_SUBDIR}"
ShowInstDetails                         show
ShowUninstDetails                       show
  
;-----------------------------------------------------------------------------------------
VIAddVersionKey  'CompanyName'          '${COMPANY_NAME}'
VIAddVersionKey  'ProductName'          '${PRODUCT_NAME}'
VIAddVersionKey  'FileDescription'      '${PRODUCT_NAME}'
VIAddVersionKey  'LegalCopyright'       'Copyright (c) ${CURRENT_YEAR}, ${COMPANY_NAME}'
VIAddVersionKey  'FileVersion'          '${VERSION_SHORT}'
VIProductVersion '${VERSION_LONG}' 
  
;-----------------------------------------------------------------------------------------
!define MUI_ABORTWARNING
!define MUI_UNABORTWARNING

!define MUI_FINISHPAGE_NOAUTOCLOSE
!define MUI_UNFINISHPAGE_NOAUTOCLOSE

!define MUI_ICON "app.ico"
!define MUI_UNICON "app.ico"

!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_RIGHT
!define MUI_HEADERIMAGE_BITMAP "header.bmp"
!define MUI_HEADERIMAGE_UNBITMAP "header.bmp"
!define MUI_HEADERIMAGE_BITMAP_NOSTRETCH
!define MUI_HEADERIMAGE_UNBITMAP_NOSTRETCH

!define MUI_WELCOMEFINISHPAGE_BITMAP "welcome.bmp"
!define MUI_UNWELCOMEFINISHPAGE_BITMAP "welcome.bmp"
!define MUI_WELCOMEFINISHPAGE_BITMAP_NOSTRETCH
!define MUI_UNWELCOMEFINISHPAGE_UNBITMAP_NOSTRETCH
  
;-----------------------------------------------------------------------------------------
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

;-----------------------------------------------------------------------------------------
!insertmacro MUI_LANGUAGE "English"

;-----------------------------------------------------------------------------------------
Function VerifyUserIsAdmin  
    UserInfo::GetAccountType
    Pop $0
    ${If} $0 != "admin"
        MessageBox MB_ICONSTOP "Admin permissions required, please use right-click > Run as Administrator."
        SetErrorLevel 740 ; ERROR_ELEVATION_REQUIRED
        Quit
    ${EndIf}  
FunctionEnd

Function .onInit
    Call VerifyUserIsAdmin
FunctionEnd

Function un.onInit
FunctionEnd
  
;-----------------------------------------------------------------------------------------
!macro RemoveFile file
    Push $0
    ${TrimNewLines} '${file}' $0
    SetDetailsPrint both
    ${If} ${FileExists} '$0'
        DetailPrint 'Deleting file $0'
        SetDetailsPrint textonly
        Delete "$0"
    ${Else}
        DetailPrint 'File not found: $0'
    ${Endif}
    SetDetailsPrint lastused
    Pop $0
!macroend

!macro RemoveDir dir
    Push $0
    ${TrimNewLines} '${dir}' $0
    SetDetailsPrint both
    ${If} ${FileExists} '$0'
        DetailPrint 'Deleting directory $0'
        SetDetailsPrint textonly
        RmDir /r "$0"
    ${Else}
        DetailPrint 'Directory not found: $0'
    ${Endif}
    SetDetailsPrint lastused
    Pop $0
!macroend

;-----------------------------------------------------------------------------------------
Function WriteFiles
    DetailPrint 'Installing application files...'
    SetDetailsPrint both
    SetOutPath '$INSTDIR'
    File 'app.ico'

    ;
    ; Add here whatever else you want to be included:
    ;
    File '${SOURCE_DIR}\${EXE_BASENAME}d.exe'
    File '${SOURCE_DIR}\${EXE_BASENAME}-wallet-cli.exe'
    File '${SOURCE_DIR}\${EXE_BASENAME}-wallet-rpc.exe'
    File '${SOURCE_DIR}\${EXE_BASENAME}-gen-trusted-multisig.exe'

    ;
    ; NOTE: you can also add all files in a dir, like this:
    ;
    ;File /r '${SOURCE_DIR}\*.*'

    SetDetailsPrint lastused
    DetailPrint 'Writing uninstaller...'
    WriteUninstaller '$INSTDIR\${UNINSTALLER_NAME}'
FunctionEnd

Function un.WriteFiles
    DetailPrint 'Removing application files...'
    !insertmacro RemoveDir '$INSTDIR'
FunctionEnd

;-----------------------------------------------------------------------------------------
Function WriteShortcuts
    DetailPrint 'Creating Desktop shortcuts...'
    CreateShortCut '$DESKTOP\${PRODUCT_NAME} Wallet.lnk' '$INSTDIR\${EXE_BASENAME}-wallet-cli.exe' '' '$INSTDIR\app.ico'
    CreateShortCut '$DESKTOP\${PRODUCT_NAME} Daemon.lnk' '$INSTDIR\${EXE_BASENAME}d.exe'           '' '$INSTDIR\app.ico'
    DetailPrint 'Creating Start Menu shortcuts...'
    CreateDirectory '$SMPROGRAMS\${COMPANY_NAME}\${PRODUCT_NAME}'
    CreateShortCut  '$SMPROGRAMS\${COMPANY_NAME}\${PRODUCT_NAME}\${PRODUCT_NAME} Wallet.lnk'    '$INSTDIR\${EXE_BASENAME}-wallet-cli.exe' '' '$INSTDIR\app.ico'
    CreateShortCut  '$SMPROGRAMS\${COMPANY_NAME}\${PRODUCT_NAME}\${PRODUCT_NAME} Daemon.lnk'    '$INSTDIR\${EXE_BASENAME}d.exe'           '' '$INSTDIR\app.ico'
    CreateShortCut  '$SMPROGRAMS\${COMPANY_NAME}\${PRODUCT_NAME}\Uninstall ${PRODUCT_NAME}.lnk' '$INSTDIR\${UNINSTALLER_NAME}'            '' '$INSTDIR\app.ico'
FunctionEnd

Function un.WriteShortcuts
    DetailPrint 'Removing Desktop shortcuts...'
    !insertmacro RemoveFile '$DESKTOP\${PRODUCT_NAME} Daemon.lnk'
    !insertmacro RemoveFile '$DESKTOP\${PRODUCT_NAME} Wallet.lnk'
    DetailPrint 'Removing Start Menu shortcuts...'
    !insertmacro RemoveDir '$SMPROGRAMS\${COMPANY_NAME}\${PRODUCT_NAME}'
FunctionEnd
  
;-----------------------------------------------------------------------------------------
Function WriteUninstaller
    DetailPrint 'Registering ${PRODUCT_NAME} ${VERSION_SHORT}'
    WriteRegDWORD HKLM '${REG_KEY}' 'VersionMajor'          ${VERSION_MAJOR}
    WriteRegDWORD HKLM '${REG_KEY}' 'VersionMinor'          ${VERSION_MINOR}
    WriteRegDWORD HKLM '${REG_KEY}' 'EstimatedSize'         ${INSTALL_SIZE}
    WriteRegDWORD HKLM '${REG_KEY}' 'NoModify'              1
    WriteRegDWORD HKLM '${REG_KEY}' 'NoRepair'              1
    WriteRegStr   HKLM '${REG_KEY}' 'Contact'              '${COMPANY_NAME}'
    WriteRegStr   HKLM '${REG_KEY}' 'Publisher'            '${COMPANY_NAME}'
    WriteRegStr   HKLM '${REG_KEY}' 'HelpLink'             '${HELP_URL}'
    WriteRegStr   HKLM '${REG_KEY}' 'URLInfoAbout'         '${ABOUT_URL}'
    WriteRegStr   HKLM '${REG_KEY}' 'DisplayVersion'       '${VERSION_SHORT}'
    WriteRegStr   HKLM '${REG_KEY}' 'Comments'             '${PRODUCT_NAME}'
    WriteRegStr   HKLM '${REG_KEY}' 'DisplayName'          '${PRODUCT_NAME} ${VERSION_SHORT}'
    WriteRegStr   HKLM '${REG_KEY}' 'InstallLocation'      '"$INSTDIR"'
    WriteRegStr   HKLM '${REG_KEY}' 'DisplayIcon'          '"$INSTDIR\app.ico"'
    WriteRegStr   HKLM '${REG_KEY}' 'UninstallString'      '"$INSTDIR\${UNINSTALLER_NAME}"'
    WriteRegStr   HKLM '${REG_KEY}' 'QuietUninstallString' '"$INSTDIR\${UNINSTALLER_NAME}" /S'
FunctionEnd

Function un.WriteUninstaller
    DetailPrint 'Unregistering ${PRODUCT_NAME} ${VERSION_SHORT}...'
    DeleteRegKey HKLM '${REG_KEY}'
FunctionEnd

;-----------------------------------------------------------------------------------------
Section "${PRODUCT_NAME} ${VERSION_SHORT}"
    Call WriteFiles
    Call WriteShortcuts
    Call WriteUninstaller
SectionEnd

;-----------------------------------------------------------------------------------------
Section "Uninstall"
    Call un.WriteFiles
    Call un.WriteShortcuts
    Call un.WriteUninstaller
SectionEnd

