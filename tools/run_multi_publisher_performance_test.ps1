param(
    [int]$PublisherCount = 1,
    [int]$Workers = 2,
    [int]$SymbolsPerPublisher = 256,
    [int]$BatchSize = 1024,
    [int]$AckWindowBatches = 4,
    [int]$QueueCapacity = 8192,
    [int]$ProcessorMaxBatchSize = 2048,
    [int]$RuntimeSeconds = 4,
    [string]$QueueType = "blocking_bounded",
    [string]$Configuration = "Release"
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
$resultsDir = Join-Path $repoRoot "results\multi_publisher_${PublisherCount}_$timestamp"
New-Item -ItemType Directory -Force $resultsDir | Out-Null

function Write-ProcessorConfig {
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
"@ | Set-Content -LiteralPath $processorIni -NoNewline
}

function Write-PublisherConfig {
    param(
        [string]$Path,
        [int]$SymbolOffset
    )

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
"@ | Set-Content -LiteralPath $Path -NoNewline
}

function Match-Value {
    param(
        [string]$Text,
        [string]$Pattern
    )

    $match = [regex]::Match($Text, $Pattern, [System.Text.RegularExpressions.RegexOptions]::Multiline)
    if ($match.Success) {
        return $match.Groups[1].Value
    }

    return ""
}

try {
    Write-ProcessorConfig

    $processorOut = Join-Path $resultsDir "processor.out"
    $processorErr = Join-Path $resultsDir "processor.err"

    Write-Host "Starting processor for $PublisherCount publisher(s)..." -ForegroundColor Yellow
    $processor = Start-Process `
        -FilePath $processorExe `
        -WorkingDirectory $repoRoot `
        -RedirectStandardOutput $processorOut `
        -RedirectStandardError $processorErr `
        -PassThru

    Start-Sleep -Milliseconds 300

    $publishers = @()
    for ($i = 0; $i -lt $PublisherCount; ++$i) {
        $publisherConfig = Join-Path $resultsDir "publisher_$i.ini"
        $publisherOut = Join-Path $resultsDir "publisher_$i.out"
        $publisherErr = Join-Path $resultsDir "publisher_$i.err"
        Write-PublisherConfig -Path $publisherConfig -SymbolOffset ($i * $SymbolsPerPublisher)

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

    $processorText = Get-Content $processorOut -Raw
    $publisherRows = @()
    foreach ($publisher in $publishers) {
        $text = Get-Content $publisher.Out -Raw
        $publisherRows += [pscustomobject]@{
            publisher = $publisher.Index
            generated = Match-Value $text "Generated events: (\d+)"
            accepted = Match-Value $text "Accepted by processor: (\d+)"
            elapsedSec = Match-Value $text "Elapsed seconds: ([0-9.]+)"
            framesPerSec = Match-Value $text "Encode throughput: ([0-9.]+)"
        }
    }

    $publisherRows | Export-Csv -NoTypeInformation -Path (Join-Path $resultsDir "publishers.csv")

    $queueDrops = ([regex]::Matches($processorText, "Drop count: (\d+)") |
        ForEach-Object { [int64]$_.Groups[1].Value } |
        Measure-Object -Sum).Sum
    $queueMax = ([regex]::Matches($processorText, "Max depth: (\d+)") |
        ForEach-Object { [int64]$_.Groups[1].Value } |
        Measure-Object -Maximum).Maximum
    if ($null -eq $queueDrops) { $queueDrops = 0 }
    if ($null -eq $queueMax) { $queueMax = 0 }

    $summary = [pscustomobject]@{
        publisherCount = $PublisherCount
        workers = $Workers
        symbolsPerPublisher = $SymbolsPerPublisher
        batchSize = $BatchSize
        ackWindowBatches = $AckWindowBatches
        queueCapacity = $QueueCapacity
        queueType = $QueueType
        processorMaxBatchSize = $ProcessorMaxBatchSize
        publisherGenerated = ($publisherRows | Measure-Object -Property generated -Sum).Sum
        publisherAccepted = ($publisherRows | Measure-Object -Property accepted -Sum).Sum
        processorReceived = Match-Value $processorText "Received count: (\d+)"
        processorSubmitted = Match-Value $processorText "Submitted count: (\d+)"
        processorRejected = Match-Value $processorText "Rejected count: (\d+)"
        processorProcessed = Match-Value $processorText "Processed count: (\d+)"
        processorProcessedPerSec = Match-Value $processorText "Processed throughput: ([0-9.]+)"
        decodeFailures = Match-Value $processorText "Decode failures: (\d+)"
        rejectedMessages = Match-Value $processorText "Rejected messages: (\d+)"
        sequenceGaps = Match-Value $processorText "Sequence gaps: (\d+)"
        p50LatencyNs = Match-Value $processorText "P50 latency: (\d+) ns"
        p95LatencyNs = Match-Value $processorText "P95 latency: (\d+) ns"
        p99LatencyNs = Match-Value $processorText "P99 latency: (\d+) ns"
        p99QueueWaitNs = Match-Value $processorText "P99 queue wait: (\d+) ns"
        queueDrops = $queueDrops
        queueMaxDepth = $queueMax
        directory = $resultsDir
    }

    $summary | Export-Csv -NoTypeInformation -Path (Join-Path $resultsDir "summary.csv")
    $summary | Format-List
}
finally {
    Set-Content -LiteralPath $processorIni -Value $processorBackup -NoNewline
    Set-Content -LiteralPath $publisherIni -Value $publisherBackup -NoNewline
}
