@echo off
setlocal
set "SRC_EXE=%~dp0mimo_hub_juce\build\MiMOHub_artefacts\Release\MiMO Hub.exe"
set "LOCAL_DIR=%LOCALAPPDATA%\MiMOHub"
set "DEST_EXE=%LOCAL_DIR%\MiMO Hub.exe"

if not exist "%LOCAL_DIR%" mkdir "%LOCAL_DIR%"

:: Termina istanze precedenti per sbloccare la sovrascrittura del file e liberare la porta 8088
taskkill /F /IM "MiMO Hub.exe" >nul 2>&1
powershell -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name = 'python.exe'\" | Where-Object { $_.CommandLine -like '*mediapipe_worker*' } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }" >nul 2>&1
ping 127.0.0.1 -n 2 >nul

if exist "%SRC_EXE%" (
    copy /y "%SRC_EXE%" "%DEST_EXE%" >nul 2>&1
)

if exist "%DEST_EXE%" (
    start "" "%DEST_EXE%"
) else if exist "%SRC_EXE%" (
    start "" "%SRC_EXE%"
) else (
    echo [ERRORE] Eseguibile MiMO Hub non trovato.
    pause
)
