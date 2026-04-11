@echo off
setlocal

cd /d "%~dp0"

if exist build (
    rmdir /s /q build
)

mkdir build

powershell -Command "Write-Host 'Configuring project...' -ForegroundColor Cyan"
cmake -S . -B build
if errorlevel 1 goto :fail_configure

powershell -Command "Write-Host 'Building Debug...' -ForegroundColor Yellow"
cmake --build build --config Debug
if errorlevel 1 goto :fail_debug

powershell -Command "Write-Host 'Building Release...' -ForegroundColor Yellow"
cmake --build build --config Release
if errorlevel 1 goto :fail_release

powershell -Command "Write-Host 'Running tests...' -ForegroundColor Yellow"
ctest --test-dir build -C Debug --output-on-failure
if errorlevel 1 goto :fail_tests

powershell -Command "Write-Host 'Build succeeded.' -ForegroundColor Green"
echo Debug executable: build\Debug\market_data_processor.exe
echo Release executable: build\Release\market_data_processor.exe
echo Test executable: build\Debug\market_data_processor_tests.exe
exit /b 0

:fail_configure
powershell -Command "Write-Host 'Configure failed.' -ForegroundColor Red"
exit /b 1

:fail_debug
powershell -Command "Write-Host 'Debug build failed.' -ForegroundColor Red"
exit /b 1

:fail_release
powershell -Command "Write-Host 'Release build failed.' -ForegroundColor Red"
exit /b 1

:fail_tests
powershell -Command "Write-Host 'Tests failed.' -ForegroundColor Red"
exit /b 1
