# Architecture

The system is organized around an external publisher application, a processor application, and a shared protocol library.

```text
market_data_publisher
  -> IPublisherSource
  -> MarketDataEvent
  -> mdp_protocol encoder
  -> TcpPublisherClient
  -> TCP connection

market_data_processor
  -> TcpEventReceiver
  -> IEventRouter
  -> WorkerPool
  -> EventProcessor
  -> state/stats/rules/metrics
```

## Boundaries

### Public API

Public domain and protocol headers live under `include/api`.

```text
include/api/domain/
  Types.h
  MarketDataEvent.h

include/api/protocol/
  MarketDataEventFrame.h
  PortProtocol.h
```

`MarketDataEvent` is the in-memory domain event. `MarketDataEventFrame` is the fixed-size wire representation. Keeping them separate prevents internal processor fields from becoming accidental wire-protocol commitments.

### Protocol Library

`lib/protocol` builds the shared `mdp_protocol` static library.

Current responsibility:

- Encode `MarketDataEvent` into `MarketDataEventFrame`.
- Decode `MarketDataEventFrame` into `MarketDataEvent`.
- Define shared port protocol structs and constants.

Both applications link this library. Protocol encoding and decoding should stay centralized here.

### Processor

Processor internals live under `src`.

```text
src/config      Processor INI parser.
src/network     Multi-client TCP event receiver.
src/pipeline    WorkerPool, queue implementations, event router.
src/processing  Validation, sequence tracking, state, stats, rules.
src/metrics     Latency and counter collection.
src/output      Processed-event, alert, and state-change sinks.
```

The processor runtime is network-only. It accepts publisher connections, validates protocol messages, reads whole event batches, stamps a batch ingest timestamp, decodes frames, updates batch-level counters, and submits events into the worker pool.

### Publisher

Publisher code lives under `publisher`.

Current responsibility:

- Load `publisher/config/publisher.ini`.
- Generate synthetic market data.
- Support configurable symbol count and symbol offset for sharded publishers.
- Encode generated events using `mdp_protocol`.
- Connect to the processor.
- Send `ClientHello`.
- Send `EventBatch` messages.
- Pipeline ACK waits with configurable `ack_window_batches`.
- Drain pending ACKs before shutdown.

The publisher is a controllable traffic generator, not a second processing engine.

## Ingestion Model

The receiver uses one accept thread plus one session thread per connected publisher, up to `network.max_connections`.

```text
publisher 0 -> TCP session thread 0
publisher 1 -> TCP session thread 1
publisher N -> TCP session thread N

session threads
  -> whole-batch receive
  -> frame decode
  -> batch ingest timestamp
  -> WorkerPool::submit
```

This mirrors common feed/session-level scaling in real market-data systems. Publishers can be sharded by symbol range:

```text
publisher 0: symbol_offset=0,   symbol_count=256
publisher 1: symbol_offset=256, symbol_count=256
```

## Processing Model

Symbols are deterministically routed to worker partitions. Each worker owns local state, statistics, sequence tracking, risk evaluation, processing latency recording, and queue wait latency recording for the symbols routed to that worker.

```text
WorkerPool::submit
  -> hash(symbol) % worker_count
  -> per-worker queue
  -> EventProcessor
  -> validation
  -> sequence tracking
  -> risk rules
  -> state update
  -> symbol stats update
  -> latency metrics
```

This keeps same-symbol processing ordered while allowing cross-symbol parallelism.

## Port Protocol

The shared port protocol is defined in `include/api/protocol/PortProtocol.h`.

Default endpoint:

```text
address: 127.0.0.1
port:    19000
```

Message envelope:

```text
MessageHeader
payload bytes
```

Message types:

- `ClientHello`
- `ServerHello`
- `EventBatch`
- `Heartbeat`
- `Ack`
- `Reject`

Event batch layout:

```text
MessageHeader(type = EventBatch)
EventBatchPayloadHeader
MarketDataEventFrame[eventFrameCount]
```

Current default max event frames per batch is `2048`. The default event frame size is `56` bytes.

## Design Rules

- Keep wire protocol structs shared and versioned.
- Keep processor-only logic out of the publisher.
- Keep publisher source adapters behind `IPublisherSource`.
- Do not duplicate frame encoding/decoding.
- Keep same-symbol state single-writer through deterministic routing.
- Prefer explicit boundaries: source, protocol, transport, router, queue, processor.
- Use bounded queues and explicit counters for overload visibility.

## Known Gaps

- No automated loopback integration test launches both executables yet.
- Heartbeat, reconnect, timeout, and `Reject` handling are incomplete.
- Ingress metrics are still mostly aggregate counters.
- Socket buffer and TCP options are not configurable yet.
