# Performance Notes

The current system is optimized around networked ingestion from external publisher processes. The old in-process synthetic producer and its config matrix have been removed so benchmarks exercise the same boundary as the processor runtime.

## Current Findings

Single-session ingestion improved with:

- whole-batch receive in `TcpEventReceiver`
- one ingest timestamp per batch
- batch-level counter updates
- ACK pipelining in the publisher

Multi-session ingestion produced the largest gain:

```text
2 publishers + 4 processor workers -> about 1.97M processed events/sec
```

Four publishers did not improve the earlier matrix and increased latency, which suggests CPU scheduling, queue contention, or partition pressure becomes the next bottleneck after two publisher sessions.

## Useful Metrics

Current useful metrics:

- accepted connections
- received/sec
- submitted/sec
- processed/sec
- rejected/dropped events
- decode failures
- rejected messages
- sequence gaps, duplicates, out-of-order events
- queue current depth
- queue max depth
- per-queue drop count
- processing latency percentiles
- queue wait latency percentiles
- publisher generated/sec
- publisher accepted count

## Metrics To Add

- bytes received/sec
- bytes sent/sec
- batches received/sec
- batches sent/sec
- average and max batch size
- socket disconnects/reconnects
- receive-to-decode latency
- decode-to-submit latency
- receive-to-enqueue latency
- ACK latency
- publisher send failures
- publisher reconnect count

The goal is to separate bottlenecks:

```text
source generation
  -> encode
  -> socket send
  -> socket receive
  -> decode
  -> queue submit
  -> queue wait
  -> processing
```

## Current Best Defaults

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

These defaults favor throughput over the lowest possible tail latency.

## Tuning Guidance

Tune in this order:

1. Publisher count and worker count.
2. Publisher batch size and processor max batch size.
3. ACK window size.
4. Queue capacity.
5. Queue implementation.
6. Symbol count and symbol sharding.
7. Socket send/receive buffer sizes once configurable.

Avoid judging throughput from a single publisher if the target workload expects multiple feed sessions. One receiver session has a different bottleneck profile than multiple sharded publisher sessions.

## Current Risks

1. Multi-executable loopback behavior is covered by scripts, not automated CTest integration.
2. `Reject`, heartbeat, reconnect, and timeout handling are incomplete.
3. Run output is text logs rather than structured benchmark CSVs.
4. Socket buffer sizing and TCP options are not configurable yet.
5. High-throughput defaults increase latency percentiles compared with smaller batches.

## Next Work

### Correctness

- Add CTest integration that launches processor and publisher together.
- Add malformed `MessageHeader` tests.
- Add oversized batch rejection tests.
- Add protocol version rejection tests.
- Add reconnect and heartbeat timeout tests.

### Performance

- Add structured interval-level CSV output.
- Add plots for receive throughput, decode failures, batch size, and queue wait.
- Add socket send/receive buffer configuration.
- Evaluate batch enqueue APIs in `WorkerPool` only after better ingress metrics are available.

### Operations

- Keep startup config printing in both executables.
- Keep run scripts writing per-process stdout/stderr logs under `results/`.
- Preserve the `.bat` entrypoints for Windows users and keep PowerShell for process orchestration.
