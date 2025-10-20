@echo off
REM Launcher script for Windows (double-click to run)
cd /d "%~dp0"
python generate_headers_gui.py
pause

