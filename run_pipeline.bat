@echo off
setlocal

cd /d "%~dp0"

powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_pipeline.ps1 %*
exit /b %ERRORLEVEL%
