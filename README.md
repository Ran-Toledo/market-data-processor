# Market Data Processor

A C++ market-data processing pipeline focused on low-latency ingestion, deterministic per-symbol routing, bounded queueing, event validation, per-symbol state/statistics, and performance benchmarking.

The project separates the processor from the market-data publisher boundary:

- `market_data_processor`: the processing application and TCP event receiver.
- `market_data_publisher`: an external synthetic traffic-generator application under `publisher/`.
- `mdp_protocol`: a shared static protocol library under `lib/protocol`.
- `include/api`: public domain and wire-protocol headers used by both apps.

The processor executable listens for TCP batches from the external publisher. Synthetic market-data generation lives in the publisher process, not inside the processor runtime.

## Repository Layout

```text
include/api/
  domain/        Shared market-data domain types.
  protocol/      Shared wire-frame and port protocol API.

lib/protocol/    Shared protocol implementation, built as mdp_protocol.

src/             Processor application internals.
  config/        Processor config parser.
  network/       TCP event receiver.
  pipeline/      WorkerPool, queues, event router.
  processing/    Validation, sequence tracking, state, stats, rules.
  metrics/       Latency and throughput counters.
  output/        Event sink interfaces.

publisher/       External publisher traffic generator.
  config/        Publisher config.
  src/           Publisher app, source interface, synthetic source, TCP client.

tests/           Unit, integration, and performance test runner.
tools/           Offline performance analysis tools.
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

## Run Processor And Publisher

Start the processor first:

```powershell
.\build\Release\market_data_processor.exe
```

Then start the publisher in another terminal:

```powershell
.\build\publisher\Release\market_data_publisher.exe
```

Default TCP endpoint:

```text
127.0.0.1:19000
```

Processor configuration:

```text
market-data-processor.ini
```

Publisher configuration:

```text
publisher\config\publisher.ini
```

The publisher generates synthetic events, encodes them with `mdp_protocol`, sends `EventBatch` messages over TCP, and waits for processor `Ack` messages.

## Networked Performance Smoke Test

```powershell
.\build.bat
.\run_performance_tests.bat
```

The performance script launches the processor and external publisher together, captures both outputs under `results/`, and reports the end-to-end TCP ingestion path. Use this path for throughput and latency tuning.

## Current Capabilities

- TCP publisher-to-processor event batch transport.
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
- Networked publisher/processor smoke testing.

## Suggested Next Improvements

1. Add automated loopback integration tests for publisher-to-processor TCP batches.
2. Add reconnect, heartbeat, and timeout behavior.
3. Add publisher handling for server `Reject` messages.
4. Add ingress metrics: bytes/sec, batches/sec, socket disconnects, receive-to-enqueue latency.
5. Add networked benchmark CSV output for publisher and processor interval samples.
