; NSIS installer script for libwebsockets

!include "MUI.nsh"

Name "libwebsockets"
OutFile "libwebsockets-${VERSION}-install.exe"

InstallDir "$PROGRAMFILES\libwebsockets"

;--------------------------------
; Installer pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH


;--------------------------------
; Uninstaller pages
!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

;--------------------------------
; Languages
!insertmacro MUI_LANGUAGE "English"

;--------------------------------
; Installer sections

Section "Files" SecInstall
	SectionIn RO
	SetOutPath "$INSTDIR"
	File "..\libwebsockets-api-doc.html"
	File "..\LICENSE"
	File "..\README.md"
	File "..\README.build.md"
	File "..\README.coding.md"
	File "..\README.test-apps.md"
	File /nonfatal "..\build\bin\Release\libwebsockets-test-client.exe"
	File /nonfatal "..\build\bin\Release\libwebsockets-test-echo.exe"
	File /nonfatal "..\build\bin\Release\libwebsockets-test-fraggle.exe"
	File /nonfatal "..\build\bin\Release\libwebsockets-test-ping.exe"
	File /nonfatal "..\build\bin\Release\libwebsockets-test-server.exe"
	File /nonfatal "..\build\bin\Release\libwebsockets-test-server-extpoll.exe"
	File /nonfatal "..\build\bin\Release\websockets.dll"
	File /nonfatal "..\build\bin\Release\websockets_shared.dll"

	SetOutPath "$INSTDIR\libwebsockets-test-server"
	File /nonfatal "..\build\bin\share\libwebsockets-test-server\favicon.ico"
	File /nonfatal "..\build\bin\share\libwebsockets-test-server\leaf.jpg"
	File /nonfatal "..\build\bin\share\libwebsockets-test-server\libwebsockets.org-logo.png"
	File /nonfatal "..\build\bin\share\libwebsockets-test-server\libwebsockets-test-server.key.pem"
	File /nonfatal "..\build\bin\share\libwebsockets-test-server\libwebsockets-test-server.pem"
	File /nonfatal "..\build\bin\share\libwebsockets-test-server\test.html"

	SetOutPath "$INSTDIR\lib"
	File /nonfatal "..\build\lib\Release\websockets.lib"
	File /nonfatal "..\build\lib\Release\websockets_static.lib"
	File /nonfatal "..\build\lib\Release\websockets_shared.lib"
	File /nonfatal "..\build\lib\Release\websockets.lib"

	SetOutPath "$INSTDIR\include"
	File "..\lib\libwebsockets.h"

	WriteUninstaller "$INSTDIR\Uninstall.exe"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\libwebsockets" "DisplayName" "libwebsockets library and clients"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\libwebsockets" "UninstallString" "$\"$INSTDIR\Uninstall.exe$\""
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\libwebsockets" "QuietUninstallString" "$\"$INSTDIR\Uninstall.exe$\" /S"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\libwebsockets" "HelpLink" "https://libwebsockets.org/"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\libwebsockets" "URLInfoAbout" "http://libwebsockets.org/"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\libwebsockets" "DisplayVersion" "${VERSION}"
	WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\libwebsockets" "NoModify" "1"
	WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\libwebsockets" "NoRepair" "1"
SectionEnd

Section "Uninstall"
	Delete "$INSTDIR\libwebsockets-api-doc.html"
	Delete "$INSTDIR\LICENSE"
	Delete "$INSTDIR\README.md"
	Delete "$INSTDIR\README.build.md"
	Delete "$INSTDIR\README.coding.md"
	Delete "$INSTDIR\README.test-apps.md"
	Delete "$INSTDIR\libwebsockets-test-client.exe"
	Delete "$INSTDIR\libwebsockets-test-echo.exe"
	Delete "$INSTDIR\libwebsockets-test-fraggle.exe"
	Delete "$INSTDIR\libwebsockets-test-ping.exe"
	Delete "$INSTDIR\libwebsockets-test-server.exe"
	Delete "$INSTDIR\libwebsockets-test-server-extpoll.exe"
	Delete "$INSTDIR\websockets.dll"

	Delete "$INSTDIR\libwebsockets-test-server\favicon.ico"
	Delete "$INSTDIR\libwebsockets-test-server\leaf.jpg"
	Delete "$INSTDIR\libwebsockets-test-server\libwebsockets.org-logo.png"
	Delete "$INSTDIR\libwebsockets-test-server\libwebsockets-test-server.key.pem"
	Delete "$INSTDIR\libwebsockets-test-server\libwebsockets-test-server.pem"
	Delete "$INSTDIR\libwebsockets-test-server\test.html"
	RMDir "$INSTDIR\libwebsockets-test-server"

	Delete "$INSTDIR\lib\websockets.lib"
	Delete "$INSTDIR\lib\websockets_static.lib"
	RMDir "$INSTDIR\lib"

	Delete "$INSTDIR\include\libwebsockets.h"
	RMDir "$INSTDIR\include"

	Delete "$INSTDIR\Uninstall.exe"
	RMDir "$INSTDIR"
	DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\libwebsockets"
SectionEnd

LangString DESC_SecInstall ${LANG_ENGLISH} "The main installation."
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
	!insertmacro MUI_DESCRIPTION_TEXT ${SecInstall} $(DESC_SecInstall)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

