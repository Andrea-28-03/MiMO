@echo off
setlocal
echo ====================================================================
echo   MiMO Hub Desktop Nativo (C++20 / JUCE) - Avvio .EXE
echo ====================================================================

set "EXE_PATH=%~dp0mimo_hub_juce\build\MiMOHub_artefacts\Release\MiMO Hub.exe"

if not exist "%EXE_PATH%" (
    echo [INFO] Eseguibile non trovato. Avvio compilazione CMake/MSVC...
    call "%~dp0mimo_hub_juce\build_juce_hub.bat"
)

if exist "%EXE_PATH%" (
    echo [OK] Avvio di "MiMO Hub.exe" in corso...
    start "" "%EXE_PATH%"
) else (
    echo [ERRORE] Impossibile trovare o compilare MiMO Hub.exe.
    pause
)
