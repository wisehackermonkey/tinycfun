@echo off
net session >nul 2>&1 || (powershell -Command "Start-Process '%~f0' -Verb RunAs" & exit /b)
regsvr32 /u "%~dp0tipdemo.dll"
pause
