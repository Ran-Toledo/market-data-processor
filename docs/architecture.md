# Architecture

The project is organized around a processor application, an external publisher application, and a shared protocol library.

```text
market_data_publisher
  -> publisher source
  -> mdp_protocol encoder
  -> TCP publisher client

market_data_processor
  -> TCP event receiver
  -> IEventRouter
  -> WorkerPool
  -> EventProcessor
  -> state/stats/rules/metrics
```

## Main Boundaries

### Public API

Public headers live under `include/api`.

```text
include/api/domain/
  Types.h
  MarketDataEvent.h

include/api/protocol/
  MarketDataEventFrame.h
  PortProtocol.h
```

`MarketDataEvent` is the in-memory domain event. `MarketDataEventFrame` is the fixed-size binary wire representation. The two should remain separate so internal application fields do not accidentally become wire-protocol commitments.

### Protocol Library

The protocol implementation lives in `lib/protocol` and builds as:

```text
mdp_protocol
```

Current responsibility:

- Encode `MarketDataEvent` into `MarketDataEventFrame`.
- Decode `MarketDataEventFrame` into `MarketDataEvent`.
- Define shared port protocol structs and constants.

Both applications link this library. Protocol encoding/decoding should not be duplicated in publisher and processor code.

### Processor

Processor internals live under `src`.

Important subsystems:

- `src/config`: processor configuration.
- `src/network`: TCP event receiver.
- `src/pipeline`: routing, worker pool, queues.
- `src/processing`: validation, sequence handling, rules, state updates, stats aggregation.
- `src/metrics`: latency and counter collection.
- `src/output`: processed-event, alert, and state-change sinks.

The processor executable starts `mdp::network::TcpEventReceiver`, accepts publisher connections, decodes event batches, stamps ingest timestamps, and submits events to `IEventRouter`.

### Publisher

Publisher code lives under `publisher`.

Current responsibility:

- Load `publisher/config/publisher.ini`.
- Generate synthetic market data.
- Encode generated events using `mdp_protocol`.
- Connect to the processor.
- Send `ClientHello`.
- Send `EventBatch` messages.
- Handle `ServerHello` and per-batch `Ack`.

Future responsibility:

- Handle `Reject`, heartbeat, reconnect behavior, and alternative source adapters.

The publisher should stay a controllable traffic generator, not a second processing engine.

## Processor Event Flow

```text
TcpEventReceiver
  -> MessageHeader validation
  -> EventBatchPayloadHeader validation
  -> MarketDataEventFrame decode
  -> ingest timestamp
  -> IEventRouter
  -> WorkerPool
  -> per-worker queue
  -> EventProcessor
  -> validation
  -> sequence tracking
  -> previous-state lookup
  -> risk rules
  -> state update
  -> symbol stats update
  -> metrics
```

Symbols are deterministically routed to workers. Each worker owns local state, stats, sequence tracking, metrics, and queue wait latency recording for the symbols routed to that worker.

## Publisher Flow

```text
market_data_publisher
  -> IPublisherSource
  -> MarketDataEvent
  -> MarketDataEventFrame
  -> EventBatch
  -> TCP connection
  -> Ack
```

The processor assigns `ingestTimestampNs` when a frame is received and decoded. The worker pool assigns `enqueueTimestampNs` when the event is submitted to a worker queue.

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

## Design Rules

- Keep wire protocol structs shared and versioned.
- Keep processor-only logic out of the publisher.
- Keep publisher source adapters out of processor internals.
- Do not duplicate frame encoding/decoding.
- Keep symbol state and stats worker-local.
- Prefer explicit interfaces at boundaries: source, protocol, transport, router.
- Add tests at the protocol boundary before expanding socket behavior.

## Recommended Next Refactors

1. Add loopback integration tests that launch receiver and publisher together.
2. Add heartbeat and reconnect behavior.
3. Add `Reject` handling on the publisher side.
4. Add receive buffer and send buffer tuning options.
5. Add networked performance profiles that record ingress, decode, queue, and processing metrics separately.
