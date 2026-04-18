param(
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$processorExe = Join-Path $repoRoot "build\$Configuration\market_data_processor.exe"
$publisherExe = Join-Path $repoRoot "build\publisher\$Configuration\market_data_publisher.exe"

if (!(Test-Path $processorExe)) {
    throw "Processor executable not found: $processorExe. Run build.bat first."
}

if (!(Test-Path $publisherExe)) {
    throw "Publisher executable not found: $publisherExe. Run build.bat first."
}

$timestamp = Get-Date -Format "yyyy-MM-dd_HH-mm-ss"
$resultsDir = Join-Path $repoRoot "results\networked_$timestamp"
New-Item -ItemType Directory -Force $resultsDir | Out-Null

$processorOut = Join-Path $resultsDir "processor.out"
$processorErr = Join-Path $resultsDir "processor.err"
$publisherOut = Join-Path $resultsDir "publisher.out"
$publisherErr = Join-Path $resultsDir "publisher.err"

Write-Host "Starting processor..." -ForegroundColor Yellow
$processor = Start-Process `
    -FilePath $processorExe `
    -WorkingDirectory $repoRoot `
    -RedirectStandardOutput $processorOut `
    -RedirectStandardError $processorErr `
    -PassThru

Start-Sleep -Milliseconds 250

Write-Host "Starting publisher..." -ForegroundColor Yellow
$publisher = Start-Process `
    -FilePath $publisherExe `
    -WorkingDirectory $repoRoot `
    -RedirectStandardOutput $publisherOut `
    -RedirectStandardError $publisherErr `
    -PassThru

if (!$publisher.WaitForExit(30000)) {
    $publisher.Kill()
    $publisher.WaitForExit()
    throw "Publisher did not exit before timeout. Logs: $resultsDir"
}

if (!$processor.WaitForExit(30000)) {
    $processor.Kill()
    $processor.WaitForExit()
    throw "Processor did not exit before timeout. Logs: $resultsDir"
}

$publisher.Refresh()
$processor.Refresh()

Write-Host "Networked performance run completed." -ForegroundColor Green
Write-Host "Results directory: $resultsDir"
Write-Host ""

Write-Host "Publisher output:" -ForegroundColor Cyan
Get-Content $publisherOut

Write-Host ""
Write-Host "Processor output:" -ForegroundColor Cyan
Get-Content $processorOut

$publisherError = if (Test-Path $publisherErr) { Get-Content $publisherErr -Raw } else { "" }
$processorError = if (Test-Path $processorErr) { Get-Content $processorErr -Raw } else { "" }

if (![string]::IsNullOrWhiteSpace($publisherError)) {
    Write-Host ""
    Write-Host "Publisher stderr:" -ForegroundColor Red
    Write-Host $publisherError
}

if (![string]::IsNullOrWhiteSpace($processorError)) {
    Write-Host ""
    Write-Host "Processor stderr:" -ForegroundColor Red
    Write-Host $processorError
}

if ($null -ne $publisher.ExitCode -and $publisher.ExitCode -ne 0) {
    throw "Publisher exited with code $($publisher.ExitCode). Logs: $resultsDir"
}

if ($null -ne $processor.ExitCode -and $processor.ExitCode -ne 0) {
    throw "Processor exited with code $($processor.ExitCode). Logs: $resultsDir"
}
