# Networked Performance Tests

Performance tests use the same boundary as normal runtime:

```text
market_data_publisher -> TCP -> market_data_processor
```

The removed in-process producer runner is no longer part of the build or test tree.

## Default Run

```powershell
.\build.bat
.\run_pipeline.bat
```

The default run launches two sharded publishers and one processor:

```text
publishers:              2
processor workers:       4
symbols per publisher:   256
publisher batch size:    1024
publisher ACK window:    4
processor max batch:     2048
queue capacity:          8192 per worker
queue type:              blocking_bounded
```

Logs are written under:

```text
results\run_<timestamp>\
```

Structured CSV exports are written beside the logs:

```text
processor-metrics.csv
publisher_0-metrics.csv
publisher_1-metrics.csv
symbol-stats.csv
```

Processed event history can be enabled in `[export]` with `enable_processed_events_csv = true`; it is disabled in default performance runs to avoid making disk write throughput the benchmark bottleneck.

## Configurable Runs

`run_pipeline.bat` forwards arguments to `tools/run_pipeline.ps1`:

```powershell
.\run_pipeline.bat -PublisherCount 1 -Workers 2 -BatchSize 512
.\run_pipeline.bat -PublisherCount 4 -Workers 8 -QueueCapacity 16384
```

For more explicit benchmark runs:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_multi_publisher_performance_test.ps1 -PublisherCount 2 -Workers 4 -SymbolsPerPublisher 256 -BatchSize 1024 -AckWindowBatches 4
```

## Current Baseline

The strongest local loopback result from the current architecture:

```text
publishers:           2
workers:              4
processed events:     9,868,948
processed throughput: 1.96671e+06 events/sec
rejected events:      0
decode failures:      0
sequence gaps:        0
P50 latency:          262,144 ns
P95 latency:          524,288 ns
P99 latency:          1,048,576 ns
```

That result used true negotiated 1024-frame batches after raising the protocol default max batch size to 2048.

## Output

Each run captures:

- `processor.out`
- `processor.err`
- `publisher_<n>.out`
- `publisher_<n>.err`
- `processor-metrics.csv`
- `publisher_<n>-metrics.csv`
- `symbol-stats.csv`
- generated publisher configs for sharded runs

The processor output includes:

- accepted connections
- received/submitted/processed counts
- rejected events
- decode failures
- rejected messages
- sequence validation counters
- throughput
- latency percentiles
- queue wait latency percentiles
- per-queue depth/drop metrics

The publisher output includes:

- loaded config
- negotiated max batch size
- generated/encoded events
- accepted count from ACKs
- encode/send throughput

## Next Benchmark Work

- Record bytes/sec, batches/sec, average batch size, and socket disconnects.
- Split receive, decode, submit, queue wait, and processing latency.
- Add configurable socket send/receive buffers and TCP options.
