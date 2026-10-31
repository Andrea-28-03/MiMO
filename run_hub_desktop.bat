@echo off
setlocal
echo ====================================================================
echo   MiMO Hub Desktop Nativo (C++20 / JUCE) - Avvio .EXE
echo ====================================================================

set "SRC_EXE=%~dp0mimo_hub_juce\build\MiMOHub_artefacts\Release\MiMO Hub.exe"
set "LOCAL_DIR=%LOCALAPPDATA%\MiMOHub"
set "DEST_EXE=%LOCAL_DIR%\MiMO Hub.exe"

if not exist "%SRC_EXE%" (
    echo [INFO] Eseguibile non trovato in build. Avvio compilazione CMake/MSVC...
    call "%~dp0mimo_hub_juce\build_juce_hub.bat"
)

if not exist "%LOCAL_DIR%" mkdir "%LOCAL_DIR%"

if exist "%SRC_EXE%" (
    copy /y "%SRC_EXE%" "%DEST_EXE%" >nul 2>&1
)

if exist "%DEST_EXE%" (
    echo [OK] Avvio di "MiMO Hub.exe" (Cartella Locale con accelerazione e webcam fisica abilitata)...
    start "" "%DEST_EXE%"
) else (
    echo [ERRORE] Impossibile trovare o compilare MiMO Hub.exe.
    pause
)
