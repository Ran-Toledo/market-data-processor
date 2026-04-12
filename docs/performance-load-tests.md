# Performance Load Experiments

Performance load experiments are separate from the unit and integration test
binary because they measure throughput and latency on the current machine
instead of asserting deterministic pass/fail behavior.

Build the project first:

```powershell
.\build.bat
```

Run the full load profile set:

```powershell
.\run_performance_tests.bat
```

Each execution writes to a timestamped directory under `results`, for example
`results\results_2026-04-11_14-25-00`.

Run one profile manually:

```powershell
.\build\Release\pipeline_load_experiments.exe --config tests\perf\configs\queue_policy_block_overload.ini --out results\block-overload.csv --samples-out results\block-overload-samples.csv --repeat 3 --sample-ms 100
```

Analyze an existing results CSV:

```powershell
python tools\analyze_performance_results.py --input results\performance-load-results.csv --samples-input results\performance-load-samples.csv --summary-out results\performance-load-summary.csv --plots-dir results\plots --timeseries-plots-dir results\plots\timeseries
```

The analyzer always writes a summary CSV. If `matplotlib` is installed, it also
writes profile comparison plots and per-profile time-series plots.

## Baseline Profile

The tracked `baseline.ini` profile is tuned as a low-latency/high-throughput
reference for the synthetic source on this machine:

```ini
num_workers = 2
producer_count = 2
producer_burst_size = 100
processing_delay_us = 0
optional_busy_work_iterations = 0
queue_capacity = 1024
queue_full_policy = drop_incoming
```

In a repeat-5 candidate run, this profile processed about 2.35M events/sec with
zero drops and p99 latency in the 16us histogram bucket.

## Profile Coverage

The profile files under `tests/perf/configs` cover:

- Baseline production and processing throughput
- Worker scaling: 1, 2, 4, and 8 workers
- Producer scaling: 1, 2, 4, and 8 producers
- Queue capacity pressure: 64, 256, 2048, and 8192 entries
- CPU-style processing work: 0, 100, 1000, and 10000 busy-work iterations
- Burst pressure: 1, 10, 100, and 1000 events per 100 microsecond sleep cycle
- Queue full policy comparison between `block_producer` and `drop_incoming`

Most scaling and backpressure profiles use `optional_busy_work_iterations = 1000`
and `processing_delay_us = 0`. That keeps comparisons CPU-bound and avoids
measuring OS scheduler behavior from microsecond sleeps. The `baseline` and
`busy_work_0` profiles keep busy work disabled to measure no-extra-work
throughput.

## Metrics

Each run writes one CSV row with the key input parameters and results:

- Generated, accepted, rejected, and processed event counts
- Generated/sec, accepted/sec, and processed/sec
- Queue drops, failed enqueue attempts, max depth, and near-capacity samples
- Validation counts including invalid, duplicate, out-of-order, and sequence gap events
- Average, min, max, p50, p95, and p99 latency in nanoseconds

The runner also writes an interval samples CSV. Each sample includes:

- Generated/sec, accepted/sec, rejected/sec, and processed/sec for the interval
- Current queue depth, total queue capacity, max queue depth seen, and near-capacity queues
- Rejections, queue drops, failed enqueue attempts, sequence gaps, and duplicate events for the interval
- Per-interval p50, p95, and p99 latency in nanoseconds

Latency percentiles are calculated in the C++ metrics path from a lightweight
atomic histogram. Final run summaries use cumulative histograms. Interval
samples use histogram deltas between samples, so the time-series latency plots
show latency changes during the profile runtime rather than only end-of-run
totals.
