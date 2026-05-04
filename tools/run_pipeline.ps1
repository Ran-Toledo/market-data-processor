param(
    [string]$Configuration = "Release",
    [int]$StartupDelayMs = 300,
    [int]$PublisherCount = 2,
    [int]$Workers = 4,
    [int]$SymbolsPerPublisher = 256,
    [int]$BatchSize = 1024,
    [int]$AckWindowBatches = 4,
    [int]$QueueCapacity = 8192,
    [int]$ProcessorMaxBatchSize = 2048,
    [int]$RuntimeSeconds = 4,
    [string]$QueueType = "blocking_bounded"
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$processorExe = Join-Path $repoRoot "build\$Configuration\market_data_processor.exe"
$publisherExe = Join-Path $repoRoot "build\publisher\$Configuration\market_data_publisher.exe"
$processorIni = Join-Path $repoRoot "market-data-processor.ini"
$publisherIni = Join-Path $repoRoot "publisher\config\publisher.ini"

if (!(Test-Path $processorExe)) {
    throw "Processor executable not found: $processorExe. Run build.bat first."
}

if (!(Test-Path $publisherExe)) {
    throw "Publisher executable not found: $publisherExe. Run build.bat first."
}

$processorBackup = Get-Content $processorIni -Raw
$publisherBackup = Get-Content $publisherIni -Raw

$timestamp = Get-Date -Format "yyyy-MM-dd_HH-mm-ss"
$resultsDir = Join-Path $repoRoot "results\run_$timestamp"
New-Item -ItemType Directory -Force $resultsDir | Out-Null

function Write-ProcessorConfig {
    $processorMetricsCsv = Join-Path $resultsDir "processor-metrics.csv"
    $symbolStatsCsv = Join-Path $resultsDir "symbol-stats.csv"

    @"
[logging]
enable_event_logging = false
enable_alert_logging = false
enable_processing_stats_logging = false

[runtime]
app_runtime_seconds = 5
num_workers = $Workers
periodic_summary_interval_ms = 1000

[worker]
processing_delay_us = 0
optional_busy_work_iterations = 0
queue_capacity = $QueueCapacity
queue_full_policy = drop_incoming
queue_type = $QueueType

[network]
listen_address = 127.0.0.1
listen_port = 19000
max_batch_size = $ProcessorMaxBatchSize
max_connections = $PublisherCount

[reporting]
processing_stats_log_interval = 1000
print_processing_stats_summary = true
print_queue_metrics_summary = true
print_symbol_stats_summary = false

[export]
enable_processed_events_csv = false
processed_events_csv_path = $(Join-Path $resultsDir "processed-events.csv")
enable_processor_metrics_csv = true
processor_metrics_csv_path = $processorMetricsCsv
enable_symbol_stats_csv = true
symbol_stats_csv_path = $symbolStatsCsv
"@ | Set-Content -LiteralPath $processorIni -NoNewline
}

function Write-PublisherConfig {
    param(
        [string]$Path,
        [int]$SymbolOffset,
        [int]$PublisherIndex
    )

    $publisherMetricsCsv = Join-Path $resultsDir "publisher_$PublisherIndex-metrics.csv"

    @"
[runtime]
event_count = 0
runtime_seconds = $RuntimeSeconds
burst_size = $BatchSize
sleep_us = 0

[network]
processor_host = 127.0.0.1
processor_port = 19000
connect_retry_ms = 1000
ack_window_batches = $AckWindowBatches

[source]
source_type = synthetic
symbol_count = $SymbolsPerPublisher
symbol_offset = $SymbolOffset

[export]
enable_publisher_metrics_csv = true
publisher_metrics_csv_path = $publisherMetricsCsv
metrics_interval_ms = 1000
"@ | Set-Content -LiteralPath $Path -NoNewline
}

try {
    Write-ProcessorConfig

    $processorOut = Join-Path $resultsDir "processor.out"
    $processorErr = Join-Path $resultsDir "processor.err"

    Write-Host "Starting processor..." -ForegroundColor Yellow
    Write-Host "Run config: publishers=$PublisherCount workers=$Workers batch=$BatchSize ack_window=$AckWindowBatches symbols_per_publisher=$SymbolsPerPublisher queue_capacity=$QueueCapacity queue_type=$QueueType"
    $processor = Start-Process `
        -FilePath $processorExe `
        -WorkingDirectory $repoRoot `
        -RedirectStandardOutput $processorOut `
        -RedirectStandardError $processorErr `
        -PassThru

    Start-Sleep -Milliseconds $StartupDelayMs

    $publishers = @()
    for ($i = 0; $i -lt $PublisherCount; ++$i) {
        $publisherConfig = Join-Path $resultsDir "publisher_$i.ini"
        $publisherOut = Join-Path $resultsDir "publisher_$i.out"
        $publisherErr = Join-Path $resultsDir "publisher_$i.err"
        Write-PublisherConfig `
            -Path $publisherConfig `
            -SymbolOffset ($i * $SymbolsPerPublisher) `
            -PublisherIndex $i

        Write-Host "Starting publisher $i..." -ForegroundColor Yellow
        $process = Start-Process `
            -FilePath $publisherExe `
            -WorkingDirectory $repoRoot `
            -ArgumentList @("--config", $publisherConfig) `
            -RedirectStandardOutput $publisherOut `
            -RedirectStandardError $publisherErr `
            -PassThru

        $publishers += [pscustomobject]@{
            Index = $i
            Process = $process
            Out = $publisherOut
            Err = $publisherErr
        }
    }

    foreach ($publisher in $publishers) {
        if (!$publisher.Process.WaitForExit(30000)) {
            $publisher.Process.Kill()
            $publisher.Process.WaitForExit()
            throw "Publisher $($publisher.Index) did not exit before timeout. Logs: $resultsDir"
        }
    }

    if (!$processor.WaitForExit(30000)) {
        $processor.Kill()
        $processor.WaitForExit()
        throw "Processor did not exit before timeout. Logs: $resultsDir"
    }

    Write-Host "Pipeline run completed." -ForegroundColor Green
    Write-Host "Results directory: $resultsDir"

    foreach ($publisher in $publishers) {
        Write-Host ""
        Write-Host "Publisher $($publisher.Index) output:" -ForegroundColor Cyan
        Get-Content $publisher.Out
    }

    Write-Host ""
    Write-Host "Processor output:" -ForegroundColor Cyan
    Get-Content $processorOut

    foreach ($publisher in $publishers) {
        $publisherError = if (Test-Path $publisher.Err) {
            Get-Content $publisher.Err -Raw
        } else {
            ""
        }

        if (![string]::IsNullOrWhiteSpace($publisherError)) {
            Write-Host ""
            Write-Host "Publisher $($publisher.Index) stderr:" -ForegroundColor Red
            Write-Host $publisherError
        }

        if ($null -ne $publisher.Process.ExitCode -and $publisher.Process.ExitCode -ne 0) {
            throw "Publisher $($publisher.Index) exited with code $($publisher.Process.ExitCode). Logs: $resultsDir"
        }
    }

    $processorError = if (Test-Path $processorErr) {
        Get-Content $processorErr -Raw
    } else {
        ""
    }

    if (![string]::IsNullOrWhiteSpace($processorError)) {
        Write-Host ""
        Write-Host "Processor stderr:" -ForegroundColor Red
        Write-Host $processorError
    }

    if ($null -ne $processor.ExitCode -and $processor.ExitCode -ne 0) {
        throw "Processor exited with code $($processor.ExitCode). Logs: $resultsDir"
    }
}
finally {
    Set-Content -LiteralPath $processorIni -Value $processorBackup -NoNewline
    Set-Content -LiteralPath $publisherIni -Value $publisherBackup -NoNewline
}
