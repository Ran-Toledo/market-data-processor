# Market Data Processor

A high-throughput C++ market data ingestion and processing system with TCP publishers, binary framing, deterministic per-symbol routing, bounded worker queues, validation, risk-rule evaluation, runtime metrics, and CSV export.

## Architecture Overview

The repository has three main runtime boundaries:

- `market_data_publisher`: generates synthetic market data and sends framed batches over TCP.
- `market_data_processor`: accepts publisher connections, decodes batches, routes events to workers, validates and processes them, and exports metrics.
- `mdp_protocol`: shared frame encoding/decoding and port protocol definitions used by both executables.

At runtime, publishers connect to the processor, negotiate batch constraints, send binary event batches, receive ACKs, and keep same-symbol events ordered by routing them to the same worker partition.

See [docs/architecture.md](docs/architecture.md) for the full system walkthrough and diagram.

## Technical Highlights

- C++17 + CMake project structure.
- Synthetic market data publisher.
- External TCP publisher/processor split.
- Shared protocol library under `lib/protocol`.
- Binary framed market data batches.
- ACK batching/windowing on the publisher side.
- Multi-client ingestion on the processor side.
- Deterministic per-symbol routing.
- Per-worker bounded queues.
- Worker-local symbol state and symbol statistics.
- Sequence validation and gap detection.
- Risk rule evaluation for price jumps and large volume.
- Throughput, queue, and latency metrics.
- CSV export for publisher metrics, processor metrics, processed event history, and symbol statistics.
- Unit and loopback integration tests.

## Build

Primary environment: Windows with Visual Studio 2022 and CMake.

Build everything and run the test suite:

```powershell
.\build.bat
```

This produces:

```text
build\Debug\market_data_processor.exe
build\Release\market_data_processor.exe
build\Debug\market_data_processor_tests.exe
build\publisher\Debug\market_data_publisher.exe
build\publisher\Release\market_data_publisher.exe
```

## Run / Demo

The quickest end-to-end demo is the bundled pipeline runner:

```powershell
.\run_pipeline.bat
```

Default run shape:

```text
2 publisher processes
4 processor workers
1024-event publisher batches
ACK window of 4 batches
256 symbols per publisher
8192 events per worker queue
```

The run writes artifacts under:

```text
results\run_<timestamp>\
```

Manual execution is also supported:

```powershell
.\build\Release\market_data_processor.exe --config .\market-data-processor.ini
.\build\publisher\Release\market_data_publisher.exe --config .\publisher\config\publisher.ini
```

For multiple manual publishers, run more than one publisher process and give them disjoint `symbol_offset` ranges in their config files.

## Example Output

Example processor summary from a recent local loopback run:

```text
Accepted connections: 2
Received count: 9682744
Submitted count: 9682744
Rejected count: 0
Processed count: 9682744
Processed throughput: 1.93461e+06 events/sec
P50 latency: 262144 ns
P95 latency: 524288 ns
P99 latency: 1048576 ns
P99 queue wait: 262144 ns
```

CSV artifacts from the same run:

```text
processor-metrics.csv
publisher_0-metrics.csv
publisher_1-metrics.csv
symbol-stats.csv
```

Processed event history export exists but is disabled by default in the high-throughput config because writing one row per processed event changes the profile of the run.

## Benchmark Summary

Benchmarking in this repo is currently aimed at local comparative testing rather than publishing a fixed set of canonical numbers.

Useful dimensions to compare:

| Scenario | Publishers | Workers | Batch Size | Duration | Throughput | Drops | Rejects | Notes |
|---|---:|---:|---:|---:|---:|---:|---:|---|
| Local baseline | TBD | TBD | TBD | TBD | Fill after benchmark | TBD | TBD | Single-publisher starting point |
| Multi-worker local | TBD | TBD | TBD | TBD | Fill after benchmark | TBD | TBD | Compare scaling with worker count |
| Multi-publisher local | TBD | TBD | TBD | TBD | Fill after benchmark | TBD | TBD | Compare scaling with more sessions |

See [docs/benchmarks.md](docs/benchmarks.md), [docs/performance-load-tests.md](docs/performance-load-tests.md), and [docs/performance-notes.md](docs/performance-notes.md).

## Project Structure

```text
include/api/
  domain/        Shared market-data domain types.
  protocol/      Shared wire-format and port protocol headers.

lib/protocol/    Shared frame encode/decode implementation.

src/
  config/        Processor config parsing.
  metrics/       Counters and latency recorders.
  network/       TCP receiver and session handling.
  output/        Console and CSV sinks.
  pipeline/      Worker pool, event router, queue implementations.
  processing/    Validation, sequence tracking, symbol state/stats, rules.

publisher/
  config/        Publisher config files.
  src/           Publisher app, synthetic source, TCP client.

tests/
  unit/          Core component tests.
  integration/   Loopback test that launches processor + publisher.

tools/           Run scripts and benchmark helpers.
docs/            Architecture, benchmarks, design notes, demo, release docs.
results/         Local benchmark and demo artifacts.
```

## Tests

The project includes:

- Unit tests for sequence tracking, event processor behavior, event queue behavior, latency recording, metrics, protocol framing, rule evaluation, symbol state, and symbol statistics.
- A loopback integration test that launches the real processor and publisher executables together and verifies counters plus CSV output generation.

Build and run tests:

```powershell
.\build.bat
```

Run the existing CTest suite without rebuilding:

```powershell
ctest --test-dir .\build -C Debug --output-on-failure
```

## Known Limitations

- Benchmarks are local loopback measurements, not cross-host or production-network measurements.
- Heartbeat, reconnect, timeout, and fuller `Reject` handling are defined at the protocol level but not yet hardened into a production-grade session layer.
- Persistence is CSV-first; there is no SQLite or external storage integration yet.
- The market data model is synthetic and intentionally simplified.
- The networking implementation is Windows/Winsock-centric today.
- Multi-publisher automated integration coverage is still lighter than the manual benchmark surface.
- Security, auth, deployment packaging, and operational hardening are outside the current scope.

## Future Improvements

- Transport hardening: reconnect, heartbeat/timeout enforcement, richer `Reject` handling.
- Ingress observability: bytes/sec, batches/sec, average batch size, socket-level counters.
- Transport tuning: socket buffer sizing, TCP options, deeper receive/decode/submit breakdown.
- Broader test coverage for malformed messages and multi-publisher scenarios.
- Optional structured persistence beyond CSV once the export shape stabilizes.

## Additional Docs

- [docs/architecture.md](docs/architecture.md)
- [docs/benchmarks.md](docs/benchmarks.md)
