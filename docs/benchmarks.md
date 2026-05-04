# Benchmarks

This document summarizes how local benchmarks can be run and how to record the results.

## How Benchmarks Were Run

The repository currently benchmarks the system in local loopback mode on Windows.

Useful entry points:

```powershell
.\run_pipeline.bat
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_multi_publisher_performance_test.ps1 -PublisherCount 2 -Workers 4
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_networked_performance_test.ps1
```

The benchmark boundary is the real networked pipeline:

```text
market_data_publisher -> TCP -> market_data_processor
```

That means results include:

- event generation
- frame encoding
- socket send/receive
- frame decode
- queue submission
- worker processing
- ACK handling

## Important Config Parameters

The most relevant knobs are:

- publisher count
- worker count
- symbols per publisher
- publisher batch size
- publisher ACK window
- processor max batch size
- worker queue capacity
- queue type
- runtime duration

Typical high-throughput local settings in this repo:

```text
publishers:            2
workers:               4
symbols per publisher: 256
batch size:            1024
ack window:            4
processor max batch:   2048
queue capacity:        8192
queue type:            blocking_bounded
```

## Benchmark Results

The repository does not currently commit raw benchmark output directories. Keep benchmark reporting here curated and human-maintained.

Use this table as a template after running local measurements:

| Scenario | Publishers | Workers | Batch Size | Duration | Throughput | Drops | Rejects | Notes |
|---|---:|---:|---:|---:|---:|---:|---:|---|
| Local baseline | TBD | TBD | TBD | TBD | Fill after benchmark | TBD | TBD | Single-publisher baseline |
| Multi-worker local | TBD | TBD | TBD | TBD | Fill after benchmark | TBD | TBD | Compare worker scaling |
| Multi-publisher local | TBD | TBD | TBD | TBD | Fill after benchmark | TBD | TBD | Compare multiple TCP sessions |

## What The Numbers Mean

The main interpretation is not just the top-line throughput value.

When you do record results, focus on comparative interpretation:

- how throughput changes with publisher count
- how throughput changes with worker count
- whether drops or rejects appear under load
- whether queue depth growth signals bottlenecks
- how P50/P95/P99 latency moves as throughput rises

## Limitations Of These Benchmarks

- they are local loopback runs, not network-distributed runs
- hardware details are not normalized in the repository docs
- the processor and publishers share the same host resources
- these runs are useful for relative comparison, not for making production throughput guarantees

## Suggested Future Benchmark Improvements

- record bytes/sec and batches/sec directly
- add CPU and memory snapshots during runs
- separate receive, decode, submit, queue-wait, and processing timing more explicitly
- capture cross-host runs instead of only loopback
- add a repeatable benchmark matrix with stable machine metadata
- export benchmark tables directly from structured results rather than mixing CSV summaries and console logs

## Related Files

- [docs/performance-load-tests.md](performance-load-tests.md)
- [docs/performance-notes.md](performance-notes.md)
- `tools/run_pipeline.ps1`
- `tools/run_multi_publisher_performance_test.ps1`
