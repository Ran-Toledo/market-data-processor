@echo off
setlocal

cd /d "%~dp0"

set PERF_EXE=build\Release\pipeline_load_experiments.exe
set CONFIG_DIR=tests\perf\configs
for /f %%I in ('powershell -NoProfile -Command "Get-Date -Format yyyy-MM-dd_HH-mm-ss"') do set RUN_TIMESTAMP=%%I
set RESULTS_DIR=results\results_%RUN_TIMESTAMP%
set RESULTS_CSV=%RESULTS_DIR%\performance-load-results.csv
set SAMPLES_CSV=%RESULTS_DIR%\performance-load-samples.csv
set SUMMARY_CSV=%RESULTS_DIR%\performance-load-summary.csv
set PLOTS_DIR=%RESULTS_DIR%\plots
set TIMESERIES_PLOTS_DIR=%RESULTS_DIR%\plots\timeseries

if not exist "%PERF_EXE%" (
    powershell -Command "Write-Host 'Performance executable not found. Run build.bat first.' -ForegroundColor Red"
    exit /b 1
)

if not exist "%RESULTS_DIR%" (
    mkdir "%RESULTS_DIR%"
)

powershell -Command "Write-Host 'Running performance load experiments...' -ForegroundColor Yellow"
"%PERF_EXE%" --config-dir "%CONFIG_DIR%" --out "%RESULTS_CSV%" --samples-out "%SAMPLES_CSV%" --repeat 3 --sample-ms 100
if errorlevel 1 goto :fail_perf

powershell -Command "Write-Host 'Analyzing performance results...' -ForegroundColor Yellow"
python tools\analyze_performance_results.py --input "%RESULTS_CSV%" --samples-input "%SAMPLES_CSV%" --summary-out "%SUMMARY_CSV%" --plots-dir "%PLOTS_DIR%" --timeseries-plots-dir "%TIMESERIES_PLOTS_DIR%"
if errorlevel 1 goto :fail_analysis

powershell -Command "Write-Host 'Performance run completed.' -ForegroundColor Green"
echo Results CSV: %RESULTS_CSV%
echo Interval samples CSV: %SAMPLES_CSV%
echo Summary CSV: %SUMMARY_CSV%
echo Plots directory: %PLOTS_DIR%
echo Time-series plots directory: %TIMESERIES_PLOTS_DIR%
exit /b 0

:fail_perf
powershell -Command "Write-Host 'Performance experiments failed.' -ForegroundColor Red"
exit /b 1

:fail_analysis
powershell -Command "Write-Host 'Performance analysis failed.' -ForegroundColor Red"
exit /b 1
