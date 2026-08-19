@echo off
setlocal
cd /d "%~dp0.."
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0fetch-colyseus.ps1" %*
exit /b %ERRORLEVEL%
