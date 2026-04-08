# Market Data Processor

A multithreaded C++ market-data pipeline with pluggable synthetic and replay-based input sources, bounded queueing, concurrent event processing, and latency/throughput instrumentation to evaluate performance under bursty workloads.

## Build

```powershell
cmake --preset vs2022-x64-debug
cmake --build --preset build-debug
```

## Run

```powershell
.\build\Debug\market_data_processor.exe
```
