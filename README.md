# Market Data Processor

A C++ market-data processing pipeline focused on networked ingestion, deterministic per-symbol routing, bounded queueing, validation, worker-local state/statistics, and throughput/latency measurement.

The project is split across three main runtime boundaries:

- `market_data_processor`: the processing application and TCP event receiver.
- `market_data_publisher`: an external synthetic market-data publisher under `publisher/`.
- `mdp_protocol`: a shared static protocol library under `lib/protocol`.

The processor does not generate synthetic events internally. Publishers connect over TCP, send framed event batches, and receive ACKs from the processor.

## Repository Layout

```text
include/api/
  domain/        Shared market-data domain types.
  protocol/      Shared wire-frame and port protocol headers.

lib/protocol/    Shared frame encode/decode implementation.

src/             Processor application internals.
  config/        Processor config parser.
  network/       Multi-client TCP event receiver.
  pipeline/      WorkerPool, queues, event router.
  processing/    Validation, sequence tracking, state, stats, rules.
  metrics/       Latency and throughput counters.
  output/        Event sink interfaces.

publisher/
  config/        Publisher config.
  src/           Publisher app, synthetic source, TCP client.

tests/           Unit and integration tests.
tools/           Run and benchmark helper scripts.
docs/            Architecture and performance notes.
```

## Build

On Windows with Visual Studio 2022:

```powershell
.\build.bat
```

The script configures CMake, builds Debug and Release targets, and runs the test suite.

Build outputs:

```text
build\Debug\market_data_processor.exe
build\Release\market_data_processor.exe
build\Debug\market_data_processor_tests.exe
build\publisher\Release\market_data_publisher.exe
```

## Run

Run the default processor plus publisher pipeline:

```powershell
.\run_pipeline.bat
```

The default run launches:

```text
2 publisher processes
4 processor workers
1024-event publisher batches
ACK window of 4 batches
256 symbols per publisher
8192 events per worker queue
```

The script writes logs under:

```text
results\run_<timestamp>\
```

The batch file forwards arguments to the PowerShell runner, so the run is configurable:

```powershell
.\run_pipeline.bat -PublisherCount 1 -Workers 2 -BatchSize 512
.\run_pipeline.bat -PublisherCount 4 -Workers 8 -QueueCapacity 16384
```

You can also run the executables manually:

```powershell
.\build\Release\market_data_processor.exe
.\build\publisher\Release\market_data_publisher.exe
```

## Configuration

Processor configuration:

```text
market-data-processor.ini
```

Publisher configuration:

```text
publisher\config\publisher.ini
```

Current defaults are tuned for a high-throughput local loopback run:

```text
processor workers:       4
processor max clients:   2
processor max batch:     2048
worker queue capacity:   8192
publisher batch size:    1024
publisher ACK window:    4
symbols per publisher:   256
```

Both executables print their loaded configuration, startup status, shutdown status, and final counters to stdout.

## Performance

The current best local loopback run processed nearly 2M events/sec:

```text
publishers:          2
workers:             4
processed events:    9,868,948
processed throughput: 1.96671e+06 events/sec
drops/rejects:       0
decode failures:     0
sequence gaps:       0
```

Use the multi-publisher runner for controlled experiments:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_multi_publisher_performance_test.ps1 -PublisherCount 2 -Workers 4
```

See [docs/performance-load-tests.md](docs/performance-load-tests.md) and [docs/performance-notes.md](docs/performance-notes.md).

## Current Capabilities

- Multi-client TCP publisher-to-processor ingestion.
- External synthetic publishers with configurable symbol sharding.
- ACK pipelining with configurable in-flight batch window.
- Shared binary event frame API.
- Shared port protocol structs for processor/publisher IPC.
- Deterministic symbol-to-worker routing.
- Worker-local symbol state and symbol stats.
- Sequence validation for new, duplicate, out-of-order, and gap events.
- Risk rule evaluation for price jumps and large volume.
- Configurable bounded queues.
- Queue metrics and queue wait latency metrics.
- Throughput and latency reporting.
- Unit and integration test coverage for core processing components.

## Next Improvements

1. Add reconnect, heartbeat, timeout, and publisher-side `Reject` handling.
2. Add structured CSV output for interval-level publisher and processor metrics.
3. Add ingress metrics: bytes/sec, batches/sec, socket disconnects, receive-to-enqueue latency.
4. Add socket send/receive buffer and TCP option configuration.
5. Add multi-publisher integration coverage for sharded symbols.
