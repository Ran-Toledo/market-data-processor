#include "core/AppConfig.h"
#include "pipeline/Producer.h"
#include "pipeline/WorkerPool.h"
#include "processing/EventProcessor.h"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <thread>

namespace
{
    mdp::MarketDataEvent makeEvent(
        mdp::SequenceNumber sequenceNumber,
        double price,
        std::uint32_t volume)
    {
        mdp::MarketDataEvent event;
        event.symbol = "AAPL";
        event.price = price;
        event.volume = volume;
        event.exchangeTimestampNs = 1;
        event.ingestTimestampNs = 1;
        event.sequenceNumber = sequenceNumber;
        return event;
    }
}

void runPipelineIntegrationTests()
{
    mdp::processing::EventProcessor processor;

    const auto first = processor.process(makeEvent(1, 100.0, 10));
    const auto second = processor.process(makeEvent(2, 105.0, 20));

    assert(first.processed);
    assert(second.processed);

    const auto stateSnapshot = processor.getStateSnapshot();
    const auto stateIt = stateSnapshot.find("AAPL");
    assert(stateIt != stateSnapshot.end());
    assert(stateIt->second.lastPrice == 105.0);
    assert(stateIt->second.lastVolume == 20);
    assert(stateIt->second.lastSequenceNumber == 2);

    const auto statsSnapshot = processor.getStatsSnapshot();
    const auto statsIt = statsSnapshot.find("AAPL");
    assert(statsIt != statsSnapshot.end());
    assert(statsIt->second.eventCount == 2);
    assert(statsIt->second.totalVolume == 30);
    assert(statsIt->second.minPrice == 100.0);
    assert(statsIt->second.maxPrice == 105.0);

    const auto configPath = std::filesystem::path(__FILE__)
        .parent_path()
        .parent_path()
        .parent_path()
        / "tests"
        / "perf"
        / "configs"
        / "queue_policy_block_overload.ini";
    mdp::config::loadFromFile(configPath);

    mdp::pipeline::WorkerPool workerPool(2);
    mdp::pipeline::Producer producer(workerPool);

    workerPool.start();
    producer.start();

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    const auto shutdownStart = std::chrono::steady_clock::now();
    producer.requestStop();
    workerPool.stop(false);
    producer.join();
    workerPool.join();
    const auto shutdownEnd = std::chrono::steady_clock::now();

    const auto shutdownMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            shutdownEnd - shutdownStart).count();
    assert(shutdownMs < 1000);
}
