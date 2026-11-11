@echo off
rem Project Kestrel - opens the config app in your browser.
rem Needs Python (already installed) and Chrome or Edge (for talking to the COM port).
cd /d "%~dp0tools\app"
start "Kestrel Config server (close this window when you're done)" /min python -m http.server 8642 --bind 127.0.0.1
timeout /t 1 /nobreak >nul
start "" "http://localhost:8642/"
