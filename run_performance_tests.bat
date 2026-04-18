@echo off
setlocal

cd /d "%~dp0"

powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_networked_performance_test.ps1
exit /b %ERRORLEVEL%
