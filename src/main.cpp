// main.cpp
#include "core/AppConfig.h"
#include "core/MarketDataEvent.h"
#include "pipeline/Producer.h"
#include "pipeline/WorkerPool.h"
#include "processing/SymbolStateStore.h"
#include "processing/SymbolStats.h"
#include "source/SyntheticMarketDataSource.h"

#include <chrono>
#include <iostream>
#include <thread>

int main()
{
    mdp::config::enableEventLogging = false;
    mdp::config::enableAlertLogging = false;
    mdp::config::enableProcessingStatsLogging = false;
    mdp::config::processingStatsLogInterval = 1000;
    mdp::config::sourceSleepMs = 1;
    mdp::config::appRuntimeMs = 5;
    mdp::config::numOfWorkers = 4;

    mdp::SyntheticMarketDataSource source;

    mdp::SymbolStateStore symbolStateStore;
    mdp::SymbolStats symbolStats;

    mdp::WorkerPool workerPool(
        mdp::config::numOfWorkers,
        symbolStateStore,
        symbolStats);

    mdp::Producer producer(source, workerPool);

    std::cout << "Starting pipeline..." << std::endl;

    workerPool.start();
    producer.start();

    const auto startTime = std::chrono::steady_clock::now();

    std::this_thread::sleep_for(
        std::chrono::seconds(mdp::config::appRuntimeMs));

    std::cout << "Stopping pipeline..." << std::endl;

    producer.stop();
    workerPool.stop();
    workerPool.join();

    const auto endTime = std::chrono::steady_clock::now();

    const auto elapsedMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime).count();

    const std::size_t producedCount = producer.getProducedCount();
    const std::size_t processedCount = workerPool.getProcessedCount();

    const double elapsedSeconds =
        static_cast<double>(elapsedMs) / 1000.0;

    std::cout << "Final produced count: " << producedCount << std::endl;
    std::cout << "Final processed count: " << processedCount << std::endl;

    std::cout << "Tracked symbols in state store: "
        << symbolStateStore.getTrackedSymbolCount() << std::endl;

    std::cout << "Tracked symbols in stats: "
        << symbolStats.getTrackedSymbolCount() << std::endl;

    std::cout << "Elapsed time: "
        << elapsedSeconds << " seconds" << std::endl;

    std::cout << "Average latency: "
        << workerPool.getAverageLatencyNs() << " ns" << std::endl;

    std::cout << "Min latency: "
        << workerPool.getMinLatencyNs() << " ns" << std::endl;

    std::cout << "Max latency: "
        << workerPool.getMaxLatencyNs() << " ns" << std::endl;

    if (elapsedSeconds > 0.0)
    {
        const double throughput =
            static_cast<double>(processedCount) / elapsedSeconds;

        std::cout << "Throughput: "
            << throughput << " events/sec" << std::endl;
    }
    else
    {
        std::cout << "Throughput: elapsed time too small to calculate."
            << std::endl;
    }

    const auto stateSnapshot = symbolStateStore.snapshot();
    const auto statsSnapshot = symbolStats.snapshot();

    std::cout << "\nSymbol summary:\n";

    for (const auto& [symbol, state] : stateSnapshot)
    {
        std::cout << "Symbol: " << symbol << '\n';
        std::cout << "  Last price: " << state.lastPrice << '\n';
        std::cout << "  Last volume: " << state.lastVolume << '\n';
        std::cout << "  Last sequence: " << state.lastSequenceNumber << '\n';

        const auto statsIt = statsSnapshot.find(symbol);
        if (statsIt != statsSnapshot.end())
        {
            const mdp::SymbolStatistics& stats = statsIt->second;

            std::cout << "  Event count: " << stats.eventCount << '\n';
            std::cout << "  Total volume: " << stats.totalVolume << '\n';
            std::cout << "  Min price: " << stats.minPrice << '\n';
            std::cout << "  Max price: " << stats.maxPrice << '\n';
            std::cout << "  Avg price: " << stats.averagePrice << '\n';
        }

        std::cout << '\n';
    }

    return 0;
}
