@echo off
echo ==========================================================
echo   MiMO Project - Esecuzione Test Unitari e di Integrazione
echo ==========================================================
python -m unittest discover -s tests -p "test_0*.py" -v
echo.
pause
