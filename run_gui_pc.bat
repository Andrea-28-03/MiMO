@echo off
echo ==========================================================
echo   MiMO Base Centrale - Avvio Simulatore GUI (480x320)
echo ==========================================================
echo Controlli da tastiera:
echo   - Spazio       : Sblocca/Blocca selezione Workspace (Enc 1)
echo   - W            : Cambia Workspace (STUDIO / LIVE)
echo   - Frecce SX/DX : Scorre Effetti (Enc 2)
echo   - Frecce SU/GIU: Press & Turn (+5%% / -5%%)
echo   - Invio        : Click singolo Effetti (Bypass in Live)
echo   - B            : Doppio Click Effetti (Parametri avanzati)
echo   - I            : Click Strumenti (Lista strumenti)
echo   - O            : Doppio Click Strumenti (Parametri strumento)
echo   - 1..5         : 5 Pulsanti Modulo/Scena
echo   - P            : Pedale Sustain (Scene Switcher)
echo   - H            : Connetti/Disconnetti PC Sync (MiMO Hub)
echo   - ESC / Q      : Chiudi finestra
echo ==========================================================
python -m src.ui_pygame_spi --window
pause
