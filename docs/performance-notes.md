# Performance Notes

The project now uses networked experiments with `market_data_publisher` and processor-side TCP ingestion. The old in-process producer path has been removed from the processor runtime so benchmark work follows the same boundary as production ingestion.

## Metrics To Preserve

Current useful metrics:

- Received/sec.
- Accepted/submitted/sec.
- Processed/sec.
- Rejected/dropped events.
- Queue current depth.
- Queue max depth.
- Near-capacity samples.
- Processing latency percentiles.
- Queue wait latency percentiles.
- Sequence gaps, duplicates, out-of-order events.

## Metrics To Add For Networked Ingestion

- Socket accepted connections.
- Socket disconnects/reconnects.
- Bytes received/sec.
- Frames received/sec.
- Frames decoded/sec.
- Decode failures by reason.
- Batches received/sec.
- Average and max batch size.
- Receive-to-decode latency.
- Decode-to-submit latency.
- Receive-to-enqueue latency.
- Publisher send failures.
- Publisher reconnect count.
- Publisher frames sent/sec.
- Publisher bytes sent/sec.

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

## Benchmark Guidance

Use networked experiments to tune:

- Publisher batch size.
- Socket send/receive buffer sizes.
- Processor receive loop strategy.
- Decode batching.
- Receive thread count.
- Backpressure behavior between OS socket buffers and worker queues.
- Worker count.
- Queue capacity.
- Queue implementation.
- Queue full policy.
- Per-symbol state/stats update cost.

## Current Risk Areas

1. The TCP path is manually smoke-tested but not covered by automated integration tests yet.
2. Heartbeat, reconnect, and reject handling are incomplete.
3. Networked benchmark output still needs structured interval CSVs.
4. The build script recreates the build directory and can print Visual Studio file-lock warnings after successful builds.

## Suggested Improvements

### Near Term

- Add loopback integration tests for publisher batches received by processor.
- Add heartbeat and reconnect behavior.
- Add publisher handling for server `Reject` messages.
- Add send/receive socket buffer configuration.
- Add networked performance test scripts that launch both processes.

### Performance

- Record interval-level ingress metrics alongside queue and latency samples.
- Add plots for decode failures, batch size, receive throughput, and receive-to-enqueue latency.
- Add baseline charts for the networked publisher/processor path.

### Code Structure

- Keep `mdp_protocol` small and stable.
- Consider a separate `mdp_network` library only after socket code is shared by processor and publisher.
- Keep publisher source adapters behind `IPublisherSource`.

### Correctness

- Validate partial TCP reads and writes.
- Validate malformed `MessageHeader` values.
- Validate mismatched batch payload sizes.
- Reject event batches exceeding configured max batch size.
- Add protocol version rejection tests.
- Add reconnect/heartbeat timeout tests.
