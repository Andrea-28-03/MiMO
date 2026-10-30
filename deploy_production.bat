@echo off
echo ==========================================================
echo   MiMO Project - Invio Pacchetto Produzione alla Rock Pi
echo ==========================================================
echo Inserisci la password di root della Rock Pi (192.168.1.183) quando richiesta:
scp -r "%~dp0src" root@192.168.1.183:/root/
scp -r "%~dp0boards" root@192.168.1.183:/root/
scp "%~dp0tests\test_04_integration_production.py" root@192.168.1.183:/root/
echo.
echo [OK] Pacchetto di produzione copiato con successo in /root/!
echo.
echo Per eseguire il test di collaudo hardware sulla Rock Pi:
echo   python3 /root/test_04_integration_production.py
echo.
echo Per avviare la shell grafica su display SPI (/dev/fb1):
echo   python3 -m src.ui_pygame_spi
echo.
pause
