# Networked Performance Tests

The old in-process load runner has been removed. Performance runs now use the same boundary as the processor runtime:

```text
market_data_publisher -> TCP -> market_data_processor
```

## Run

```powershell
.\build.bat
.\run_performance_tests.bat
```

The script launches the processor, waits briefly for the TCP listener, launches the publisher, and writes logs under:

```text
results\networked_<timestamp>\
```

## Current Output

- `processor.out`
- `processor.err`
- `publisher.out`
- `publisher.err`

The processor output includes connection count, received/submitted/processed counts, queue metrics, throughput, processing latency, and queue wait latency. The publisher output includes generated frame count, accepted count, elapsed seconds, and encode/send throughput.

## Next Benchmark Work

- Add structured CSV output for processor interval samples.
- Add structured CSV output for publisher send metrics.
- Record bytes/sec, batches/sec, and average batch size.
- Split receive, decode, submit, queue wait, and processing latency.
- Add configurable socket send/receive buffers.
