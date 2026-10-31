@echo off
setlocal enabledelayedexpansion

echo ====================================================================
echo   MiMO Hub - C++ JUCE Standalone Build Script
echo ====================================================================

set "CMAKE_BIN=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

if not exist "!CMAKE_BIN!" (
    where cmake >nul 2>nul
    if %errorlevel% equ 0 (
        set "CMAKE_BIN=cmake"
    ) else (
        echo [ERRORE] CMake non trovato in: !CMAKE_BIN!
        echo Assicurati che CMake sia installato o aggiunto al PATH.
        pause
        exit /b 1
    )
)

echo [1/3] Verifica strumenti completata. Utilizzo CMake: "!CMAKE_BIN!"

cd /d "%~dp0"

if not exist "build" mkdir "build"
cd build

echo [2/3] Configurazione CMake con FetchContent (JUCE 7.0.12)...
"!CMAKE_BIN!" .. -G "Visual Studio 18 2026" -A x64
if %errorlevel% neq 0 (
    echo [ERRORE] Configurazione CMake fallita.
    pause
    exit /b %errorlevel%
)

echo [3/3] Compilazione MiMO Hub in configurazione Release...
"!CMAKE_BIN!" --build . --config Release --target MiMOHub
if %errorlevel% neq 0 (
    echo [ERRORE] Compilazione fallita.
    pause
    exit /b %errorlevel%
)

echo ====================================================================
echo   BUILD COMPLETATA CON SUCCESSO!
echo   Eseguibile generato in: mimo_hub_juce\build\MiMOHub_artefacts\Release\MiMO Hub.exe
echo ====================================================================

if not exist "%LOCALAPPDATA%\MiMOHub" mkdir "%LOCALAPPDATA%\MiMOHub"
copy /y "%~dp0build\MiMOHub_artefacts\Release\MiMO Hub.exe" "%LOCALAPPDATA%\MiMOHub\MiMO Hub.exe" >nul 2>&1
echo [OK] Copia locale aggiornata in %%LOCALAPPDATA%%\MiMOHub\MiMO Hub.exe
echo.

pause
