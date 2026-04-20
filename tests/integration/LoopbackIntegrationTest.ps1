param(
    [Parameter(Mandatory = $true)]
    [string]$ProcessorExe,

    [Parameter(Mandatory = $true)]
    [string]$PublisherExe,

    [Parameter(Mandatory = $true)]
    [string]$WorkDir
)

$ErrorActionPreference = "Stop"

if (!(Test-Path $ProcessorExe)) {
    throw "Processor executable not found: $ProcessorExe"
}

if (!(Test-Path $PublisherExe)) {
    throw "Publisher executable not found: $PublisherExe"
}

$testRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("mdp_loopback_" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Force $testRoot | Out-Null

$processorConfig = Join-Path $testRoot "processor.ini"
$publisherConfig = Join-Path $testRoot "publisher.ini"
$processorOut = Join-Path $testRoot "processor.out"
$processorErr = Join-Path $testRoot "processor.err"
$publisherOut = Join-Path $testRoot "publisher.out"
$publisherErr = Join-Path $testRoot "publisher.err"
$processorMetricsCsv = Join-Path $testRoot "processor-metrics.csv"
$publisherMetricsCsv = Join-Path $testRoot "publisher-metrics.csv"
$symbolStatsCsv = Join-Path $testRoot "symbol-stats.csv"
$processedEventsCsv = Join-Path $testRoot "processed-events.csv"

function Write-ProcessorConfig {
@"
[logging]
enable_event_logging = false
enable_alert_logging = false
enable_processing_stats_logging = false

[runtime]
app_runtime_seconds = 2
num_workers = 2
periodic_summary_interval_ms = 500

[worker]
processing_delay_us = 0
optional_busy_work_iterations = 0
queue_capacity = 4096
queue_full_policy = drop_incoming
queue_type = blocking_bounded

[network]
listen_address = 127.0.0.1
listen_port = 19000
max_batch_size = 512
max_connections = 1

[reporting]
processing_stats_log_interval = 1000
print_processing_stats_summary = true
print_queue_metrics_summary = true
print_symbol_stats_summary = false

[export]
enable_processed_events_csv = true
processed_events_csv_path = $processedEventsCsv
enable_processor_metrics_csv = true
processor_metrics_csv_path = $processorMetricsCsv
enable_symbol_stats_csv = true
symbol_stats_csv_path = $symbolStatsCsv
"@ | Set-Content -LiteralPath $processorConfig -NoNewline
}

function Write-PublisherConfig {
@"
[runtime]
event_count = 5000
runtime_seconds = 0
burst_size = 128
sleep_us = 0

[network]
processor_host = 127.0.0.1
processor_port = 19000
connect_retry_ms = 100
ack_window_batches = 2

[source]
source_type = synthetic
symbol_count = 32
symbol_offset = 0

[export]
enable_publisher_metrics_csv = true
publisher_metrics_csv_path = $publisherMetricsCsv
metrics_interval_ms = 500
"@ | Set-Content -LiteralPath $publisherConfig -NoNewline
}

function Match-RequiredValue {
    param(
        [string]$Text,
        [string]$Pattern,
        [string]$Name
    )

    $match = [regex]::Match($Text, $Pattern, [System.Text.RegularExpressions.RegexOptions]::Multiline)
    if (!$match.Success) {
        throw "Missing $Name in process output. Logs: $testRoot"
    }

    return [int64]$match.Groups[1].Value
}

function Assert-Equal {
    param(
        [int64]$Actual,
        [int64]$Expected,
        [string]$Name
    )

    if ($Actual -ne $Expected) {
        throw "$Name expected $Expected but got $Actual. Logs: $testRoot"
    }
}

function Assert-CsvHasRows {
    param(
        [string]$Path,
        [string]$Name
    )

    if (!(Test-Path $Path)) {
        throw "Missing $Name CSV: $Path. Logs: $testRoot"
    }

    $rows = @(Import-Csv $Path)
    if ($rows.Count -lt 1) {
        throw "$Name CSV has no rows: $Path. Logs: $testRoot"
    }
}

try {
    Write-ProcessorConfig
    Write-PublisherConfig

    $processor = Start-Process `
        -FilePath $ProcessorExe `
        -WorkingDirectory $WorkDir `
        -ArgumentList @("--config", $processorConfig) `
        -RedirectStandardOutput $processorOut `
        -RedirectStandardError $processorErr `
        -PassThru

    Start-Sleep -Milliseconds 300

    $publisher = Start-Process `
        -FilePath $PublisherExe `
        -WorkingDirectory $WorkDir `
        -ArgumentList @("--config", $publisherConfig) `
        -RedirectStandardOutput $publisherOut `
        -RedirectStandardError $publisherErr `
        -PassThru

    if (!$publisher.WaitForExit(10000)) {
        $publisher.Kill()
        $publisher.WaitForExit()
        throw "Publisher timed out. Logs: $testRoot"
    }

    if (!$processor.WaitForExit(10000)) {
        $processor.Kill()
        $processor.WaitForExit()
        throw "Processor timed out. Logs: $testRoot"
    }

    $publisher.Refresh()
    $processor.Refresh()

    if ($null -ne $publisher.ExitCode -and $publisher.ExitCode -ne 0) {
        throw "Publisher exited with code $($publisher.ExitCode). Logs: $testRoot"
    }

    if ($null -ne $processor.ExitCode -and $processor.ExitCode -ne 0) {
        throw "Processor exited with code $($processor.ExitCode). Logs: $testRoot"
    }

    $processorText = Get-Content $processorOut -Raw
    $publisherText = Get-Content $publisherOut -Raw

    Assert-Equal (Match-RequiredValue $publisherText "Generated events: (\d+)" "publisher generated count") 5000 "publisher generated count"
    Assert-Equal (Match-RequiredValue $publisherText "Encoded frames: (\d+)" "publisher encoded count") 5000 "publisher encoded count"
    Assert-Equal (Match-RequiredValue $publisherText "Accepted by processor: (\d+)" "publisher accepted count") 5000 "publisher accepted count"
    Assert-Equal (Match-RequiredValue $publisherText "Encode failures: (\d+)" "publisher encode failures") 0 "publisher encode failures"

    Assert-Equal (Match-RequiredValue $processorText "Accepted connections: (\d+)" "processor accepted connections") 1 "processor accepted connections"
    Assert-Equal (Match-RequiredValue $processorText "Received count: (\d+)" "processor received count") 5000 "processor received count"
    Assert-Equal (Match-RequiredValue $processorText "Submitted count: (\d+)" "processor submitted count") 5000 "processor submitted count"
    Assert-Equal (Match-RequiredValue $processorText "Rejected count: (\d+)" "processor rejected count") 0 "processor rejected count"
    Assert-Equal (Match-RequiredValue $processorText "Decode failures: (\d+)" "processor decode failures") 0 "processor decode failures"
    Assert-Equal (Match-RequiredValue $processorText "Rejected messages: (\d+)" "processor rejected messages") 0 "processor rejected messages"
    Assert-Equal (Match-RequiredValue $processorText "Processed count: (\d+)" "processor processed count") 5000 "processor processed count"
    Assert-Equal (Match-RequiredValue $processorText "Sequence gaps: (\d+)" "processor sequence gaps") 0 "processor sequence gaps"

    Assert-CsvHasRows $processorMetricsCsv "processor metrics"
    Assert-CsvHasRows $publisherMetricsCsv "publisher metrics"
    Assert-CsvHasRows $symbolStatsCsv "symbol stats"
    Assert-CsvHasRows $processedEventsCsv "processed events"

    Write-Host "Loopback integration test passed. Logs: $testRoot"
}
catch {
    if (Test-Path $publisherOut) {
        Write-Host "Publisher stdout:" -ForegroundColor Cyan
        Get-Content $publisherOut
    }

    if (Test-Path $publisherErr) {
        $publisherError = Get-Content $publisherErr -Raw
        if (![string]::IsNullOrWhiteSpace($publisherError)) {
            Write-Host "Publisher stderr:" -ForegroundColor Red
            Write-Host $publisherError
        }
    }

    if (Test-Path $processorOut) {
        Write-Host "Processor stdout:" -ForegroundColor Cyan
        Get-Content $processorOut
    }

    if (Test-Path $processorErr) {
        $processorError = Get-Content $processorErr -Raw
        if (![string]::IsNullOrWhiteSpace($processorError)) {
            Write-Host "Processor stderr:" -ForegroundColor Red
            Write-Host $processorError
        }
    }

    throw
}
