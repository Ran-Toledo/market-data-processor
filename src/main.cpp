// main.cpp
#include "core/AppConfig.h"
#include "core/MarketDataEvent.h"
#include "core/ThreadSafeQueue.h"
#include "processing/EventProcessor.h"
#include "pipeline/Producer.h"
#include "source/SyntheticMarketDataSource.h"

#include <chrono>
#include <iostream>
#include <thread>

int main()
{
    mdp::config::enableEventLogging = false;
    mdp::config::enableProcessingStatsLogging = true;
    mdp::config::processingStatsLogInterval = 100;
    mdp::config::sourceSleepMs = 1;

    mdp::ThreadSafeQueue<mdp::MarketDataEvent> queue;
    mdp::SyntheticMarketDataSource source;
    mdp::Producer producer(source, queue);
    mdp::EventProcessor processor(queue);

    std::cout << "Starting pipeline..." << std::endl;

    processor.start();
    producer.start();

    const auto startTime = std::chrono::steady_clock::now();

    std::this_thread::sleep_for(std::chrono::seconds(10));

    std::cout << "Stopping pipeline..." << std::endl;

    producer.stop();
    queue.close();
    processor.stop();

    const auto endTime = std::chrono::steady_clock::now();

    const auto elapsedMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    const std::size_t producedCount = producer.getProducedCount();
    const std::size_t processedCount = processor.getProcessedCount();
    const double elapsedSeconds = static_cast<double>(elapsedMs) / 1000.0;

    std::cout << "Final produced count: " << producedCount << std::endl;
    std::cout << "Final processed count: " << processedCount << std::endl;
    std::cout << "Elapsed time: " << elapsedSeconds << " seconds" << std::endl;
    std::cout << "Average latency: " << processor.getAverageLatencyNs() << " ns" << std::endl;
    std::cout << "Min latency: " << processor.getMinLatencyNs() << " ns" << std::endl;
    std::cout << "Max latency: " << processor.getMaxLatencyNs() << " ns" << std::endl;

    if (elapsedSeconds > 0.0)
    {
        const double throughput = static_cast<double>(processedCount) / elapsedSeconds;
        std::cout << "Throughput: " << throughput << " events/sec" << std::endl;
    }
    else
    {
        std::cout << "Throughput: elapsed time too small to calculate." << std::endl;
    }

    return 0;
}
