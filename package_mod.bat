@echo off
setlocal

set "ZIP_NAME=BetterVR.zip"
set "TEMP_DIR=temp_package"

REM Clean up previous run
if exist "%TEMP_DIR%" rmdir /S /Q "%TEMP_DIR%"
if exist "%ZIP_NAME%" del "%ZIP_NAME%"

REM Create directory structure
mkdir "%TEMP_DIR%"
mkdir "%TEMP_DIR%\BreathOfTheWild_BetterVR"

REM Copy DLL
if exist "Cemu\BetterVR_Layer.dll" (
    copy "Cemu\BetterVR_Layer.dll" "%TEMP_DIR%\" >nul
    echo Copied BetterVR_Layer.dll
) else (
    echo Error: Cemu\BetterVR_Layer.dll not found.
    goto :error
)

REM Copy JSON
if exist "Cemu\BetterVR_Layer.json" (
    copy "Cemu\BetterVR_Layer.json" "%TEMP_DIR%\" >nul
    echo Copied BetterVR_Layer.json
) else (
    echo Error: Cemu\BetterVR_Layer.json not found.
    goto :error
)

REM Copy LIB
if exist "Cemu\BetterVR_Layer.lib" (
    copy "Cemu\BetterVR_Layer.lib" "%TEMP_DIR%\" >nul
    echo Copied BetterVR_Layer.lib
) else (
    echo Error: Cemu\BetterVR_Layer.lib not found.
    goto :error
)

REM Copy PDB
if exist "Cemu\BetterVR_Layer.pdb" (
    copy "Cemu\BetterVR_Layer.pdb" "%TEMP_DIR%\" >nul
    echo Copied BetterVR_Layer.pdb
) else (
    echo Error: Cemu\BetterVR_Layer.pdb not found.
    goto :error
)

REM Copy bat
if exist "resources\BetterVR LAUNCH CEMU IN VR.bat" (
    copy "resources\BetterVR LAUNCH CEMU IN VR.bat" "%TEMP_DIR%\" >nul
    echo Copied BetterVR LAUNCH CEMU IN VR.bat
) else (
    echo Error: resources\BetterVR LAUNCH CEMU IN VR.bat not found.
    goto :error
)

REM Copy compat bat
if exist "resources\BetterVR LAUNCH CEMU IN VR - COMPATIBILITY MODE.bat" (
    copy "resources\BetterVR LAUNCH CEMU IN VR - COMPATIBILITY MODE.bat" "%TEMP_DIR%\" >nul
    echo Copied BetterVR LAUNCH CEMU IN VR - COMPATIBILITY MODE.bat
) else (
    echo Error: resources\BetterVR LAUNCH CEMU IN VR - COMPATIBILITY MODE.bat not found.
    goto :error
)

REM Copy uninstall bat
if exist "resources\BetterVR UNINSTALL.bat" (
    copy "resources\BetterVR UNINSTALL.bat" "%TEMP_DIR%\" >nul
    echo Copied BetterVR UNINSTALL.bat
) else (
    echo Error: resources\BetterVR UNINSTALL.bat not found.
    goto :error
)


REM Copy Graphic Packs
if exist "resources\BreathOfTheWild_BetterVR" (
    xcopy /E /I /Y "resources\BreathOfTheWild_BetterVR" "%TEMP_DIR%\BreathOfTheWild_BetterVR" >nul
    echo Copied BreathOfTheWild_BetterVR resources
) else (
    echo Error: resources\BreathOfTheWild_BetterVR not found.
    goto :error
)

REM Create Zip
echo Creating %ZIP_NAME%...
powershell -Command "Compress-Archive -Path '%TEMP_DIR%\*' -DestinationPath '%ZIP_NAME%'"

REM Cleanup
rmdir /S /Q "%TEMP_DIR%"
echo Package created successfully: %ZIP_NAME%
goto :eof

:error
echo Failed to create package.
if exist "%TEMP_DIR%" rmdir /S /Q "%TEMP_DIR%"
exit /b 1
