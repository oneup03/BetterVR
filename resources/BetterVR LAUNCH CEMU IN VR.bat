@echo off
setlocal EnableExtensions DisableDelayedExpansion

rem Handle UTF-8 characters
chcp 65001 >nul

rem Always operate from the script directory
cd /d "%~dp0"
set "CEMU_DIR=%CD%"
set "CEMU_EXE=%CEMU_DIR%\Cemu.exe"
set "SOURCE_PACK=%CEMU_DIR%\BreathOfTheWild_BetterVR"
set "LOG=%CEMU_DIR%\BetterVR_install.log"
set "ROBOLOG=%CEMU_DIR%\robocopy_debug.log"

call :Log "=== BetterVR launcher started ==="
call :Log "Working dir: %CEMU_DIR%"

rem Check for Administrator privileges
net session >nul 2>&1
if %errorlevel% equ 0 (
  call :Log "WARN: Script running as Administrator!"
  call :Popup "Warning: You are running this script as Administrator. This will launch Cemu as Administrator, which prevents VR from working (VR runtimes cannot hook into Admin processes). Please run as a standard user." "BetterVR Warning"
)

rem 1) Hard requirement: Cemu.exe must be next to the script
if not exist "%CEMU_EXE%" (
  call :Log "ERROR: Cemu.exe not found next to script. Exiting."
  call :Popup "Cemu.exe was not found next to this script. Place this .bat next to Cemu.exe (or launch it with Cemu's folder as the working directory), then run again." "BetterVR"
  exit /b 1
)

rem 1.5) Check Cemu version (Require 2.6+)
powershell -NoProfile -Command "$v=[System.Diagnostics.FileVersionInfo]::GetVersionInfo('%CEMU_EXE%'); if ($v.FileMajorPart -lt 2 -or ($v.FileMajorPart -eq 2 -and $v.FileMinorPart -lt 6)) { exit 1 }"
if errorlevel 1 (
  call :Log "WARN: Cemu version is older than 2.6."
  call :Popup "Warning: It seems you are using an older version of Cemu. BetterVR is designed for Cemu 2.6 or newer. You might experience issues." "BetterVR Warning"
)

rem 2) Choose install target based on portable/settings.xml/appdata
set "MODE="
set "TARGET_BASE="

if exist "%CEMU_DIR%\portable\" (
  set "MODE=portable"
  set "TARGET_BASE=%CEMU_DIR%\portable\graphicPacks"
) else if exist "%CEMU_DIR%\settings.xml" (
  set "MODE=settings.xml"
  set "TARGET_BASE=%CEMU_DIR%\graphicPacks"
) else (
  set "MODE=appdata"
  set "TARGET_BASE=%APPDATA%\Cemu\graphicPacks"
)

call :Log "Install mode: %MODE%"
call :Log "Target base: %TARGET_BASE%"

rem 3) If source pack is missing, log and decide whether to continue
if exist "%SOURCE_PACK%\" goto SourcePackFound

call :Log "WARN: Source pack folder not found next to Cemu.exe: %SOURCE_PACK%"

rem Log the "user might have manually deleted graphicPacks" hint as requested
if "%MODE%"=="portable" goto CheckPortable
if "%MODE%"=="settings.xml" goto CheckSettings
goto CheckAppData

:CheckPortable
if exist "%CEMU_DIR%\portable\" if not exist "%CEMU_DIR%\portable\graphicPacks\" (
  call :Log "NOTE: portable exists but portable\graphicPacks is missing. User might have manually deleted the graphicPacks folder."
)
goto CheckDone

:CheckSettings
if exist "%CEMU_DIR%\settings.xml" if not exist "%CEMU_DIR%\graphicPacks\" (
  call :Log "NOTE: settings.xml exists but graphicPacks is missing. User might have manually deleted the graphicPacks folder."
)
goto CheckDone

:CheckAppData
if not exist "%APPDATA%\Cemu\graphicPacks\" (
  call :Log "NOTE: %APPDATA%\Cemu\graphicPacks is missing. User might have manually deleted the graphicPacks folder."
)

:CheckDone
call :CheckExistingInstall
if "%FOUND_INSTALL%"=="1" (
  call :Log "INFO: Existing BreathOfTheWild_BetterVR was found somewhere, but source pack folder is missing. Leaving existing install unchanged."
  goto LaunchCemu
)

call :Log "ERROR: No source pack folder and no existing install found in any known location."
call :Popup "BreathOfTheWild_BetterVR folder was not found next to Cemu.exe, and it is not installed in any known graphicPacks location. Put the BreathOfTheWild_BetterVR folder next to Cemu.exe, then run again." "BetterVR"
exit /b 2

:SourcePackFound

rem 4) Ensure target base exists
if not exist "%TARGET_BASE%\" (
  mkdir "%TARGET_BASE%" 2>nul
  if errorlevel 1 (
    call :Log "ERROR: Failed to create target folder: %TARGET_BASE%"
    call :Popup "Failed to create the graphicPacks folder. Check permissions and try again." "BetterVR"
    exit /b 3
  )
  call :Log "Created target base folder."
)

rem 5) Delete existing pack folder in target (delete and replace)
set "TARGET_PACK=%TARGET_BASE%\BreathOfTheWild_BetterVR"
if exist "%TARGET_PACK%\" (
  call :Log "INFO: Deleting existing target pack folder: %TARGET_PACK%"
  rmdir /s /q "%TARGET_PACK%" 2>nul
)

rem 6) Install (move semantics)
rem First try robocopy /MOVE. If that fails with a fatal code (8+), retry as copy-then-delete.
call :InstallPack "%SOURCE_PACK%" "%TARGET_PACK%"
if %RC% GEQ 8 (
  call :Log "ERROR: Install failed (robocopy RC=%RC%). Debug log kept at: %ROBOLOG%"
  call :Popup "Failed to install BreathOfTheWild_BetterVR (robocopy RC=%RC%). Open robocopy_debug.log next to Cemu.exe for the exact reason." "BetterVR"
  exit /b 4
)

call :Log "INFO: Install complete."

:LaunchCemu
rem 7) Set env vars and launch Cemu
set VK_LAYER_PATH=.\;%VK_LAYER_PATH%
set VK_INSTANCE_LAYERS=VK_LAYER_CREMENTIF_bettervr
set VK_LOADER_DEBUG=all
set VK_LOADER_DISABLE_INST_EXT_FILTER=1

call :Log "Launching Cemu.exe"

rem Check UAC status
set "UAC_DISABLED=0"
reg query "HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\CurrentVersion\Policies\System" /v "ConsentPromptBehaviorAdmin" 2>nul | find  "0x0" >NUL
if "%ERRORLEVEL%"=="0" (
  set "UAC_DISABLED=1"
  call :Log "WARN: UAC is disabled (ConsentPromptBehaviorAdmin=0x0)."
  call :Popup "Warning: UAC is disabled. This forces Cemu to run as Administrator, which may break VR functionality." "BetterVR Warning"
) else (
  call :Log "INFO: UAC is enabled."
)

:: %~1 captures the title-id of the game
set "GAME_ID=%~1"

if "%GAME_ID%"=="" (
    call :Log "No Title ID provided. Launching Cemu menu."
    start "" "%CEMU_EXE%"
) else (
    call :Log "Launching Game ID: %GAME_ID%"
    start "" "%CEMU_EXE%" -t %GAME_ID%
)

exit /b 0


:InstallPack
set "SRC=%~1"
set "DST=%~2"
set "RC=999"

call :Log "INFO: Installing pack from '%SRC%' to '%DST%'"

rem Remove any old debug log at the start of an install attempt
del /q "%ROBOLOG%" 2>nul

call :Log "INFO: Robocopy pass 1: MOVE"
robocopy "%SRC%" "%DST%" /E /MOVE /R:0 /W:0 /V /LOG:"%ROBOLOG%" >nul
set "RC=%errorlevel%"
call :Log "INFO: Robocopy MOVE RC=%RC%"

if %RC% LSS 8 goto InstallSuccess

call :Log "WARN: Robocopy MOVE failed (RC=%RC%). Retrying as COPY then delete."

rem Make sure destination is clean before retry
if exist "%DST%\" (
  rmdir /s /q "%DST%" 2>nul
)

call :Log "INFO: Robocopy pass 2: COPY"
robocopy "%SRC%" "%DST%" /E /R:0 /W:0 /V /LOG+:"%ROBOLOG%" >nul
set "RC=%errorlevel%"
call :Log "INFO: Robocopy COPY RC=%RC%"

if %RC% GEQ 8 goto InstallEnd

call :Log "INFO: COPY succeeded, deleting source folder to complete move semantics."
rmdir /s /q "%SRC%" 2>nul

:InstallSuccess
rem If overall success, delete the debug log so it only remains when there is an error
del /q "%ROBOLOG%" 2>nul

:InstallEnd
exit /b


:CheckExistingInstall
set "FOUND_INSTALL=0"
if exist "%CEMU_DIR%\portable\graphicPacks\BreathOfTheWild_BetterVR\" set "FOUND_INSTALL=1"
if exist "%CEMU_DIR%\graphicPacks\BreathOfTheWild_BetterVR\" set "FOUND_INSTALL=1"
if exist "%APPDATA%\Cemu\graphicPacks\BreathOfTheWild_BetterVR\" set "FOUND_INSTALL=1"
exit /b


:Log
>>"%LOG%" echo [%DATE% %TIME%] %1
exit /b


:Popup
set "MSG=%~1"
set "TTL=%~2"

rem Prefer PowerShell MessageBox, fall back to mshta popup
powershell -NoProfile -Command ^
  "Add-Type -AssemblyName PresentationFramework;[System.Windows.MessageBox]::Show(\"%MSG%\",\"%TTL%\") | Out-Null" 2>nul

if errorlevel 1 (
  mshta "javascript:var sh=new ActiveXObject('WScript.Shell'); sh.Popup('%MSG%',0,'%TTL%',48); close();" >nul 2>&1
)
exit /b
