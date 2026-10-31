Unicode true
!include "MUI2.nsh"

!ifndef VERSION
!define VERSION "0.0.0"
!endif
!ifndef DIST
!define DIST "dist"
!endif
!ifndef OUTDIR
!define OUTDIR "."
!endif

Name "Rabnest"
OutFile "${OUTDIR}\Rabnest-${VERSION}-windows-x64.exe"
InstallDir "$LOCALAPPDATA\Programs\Rabnest"
RequestExecutionLevel user
SetCompressor /SOLID lzma

!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "French"

Section "Install"
  SetOutPath "$INSTDIR"
  File /r "${DIST}\*"

  CreateShortCut "$SMPROGRAMS\Rabnest.lnk" "$INSTDIR\Rabnest.exe"
  CreateShortCut "$DESKTOP\Rabnest.lnk" "$INSTDIR\Rabnest.exe"

  WriteUninstaller "$INSTDIR\Uninstall.exe"

  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Rabnest" "DisplayName" "Rabnest"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Rabnest" "DisplayVersion" "${VERSION}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Rabnest" "Publisher" "Sophie Denys"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Rabnest" "UninstallString" "$INSTDIR\Uninstall.exe"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Rabnest" "DisplayIcon" "$INSTDIR\Rabnest.exe"
SectionEnd

Section "Uninstall"
  Delete "$SMPROGRAMS\Rabnest.lnk"
  Delete "$DESKTOP\Rabnest.lnk"
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Rabnest"
  RMDir /r "$INSTDIR"
SectionEnd
