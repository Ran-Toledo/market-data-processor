#include "core/AppConfig.h"
#include "core/MarketDataEvent.h"
#include "core/ThreadSafeQueue.h"
#include "processing/EventProcessor.h"
#include "source/SyntheticMarketDataSource.h"

#include <chrono>
#include <iostream>
#include <thread>

int main()
{
    mdp::config::enableEventLogging = true;
    mdp::config::enableProcessingStatsLogging = true;
    mdp::config::processingStatsLogInterval = 1000;
    mdp::config::sourceSleepMs = 1;

    mdp::ThreadSafeQueue<mdp::MarketDataEvent> queue;
    mdp::SyntheticMarketDataSource source(queue);
    mdp::EventProcessor processor(queue);

    processor.start();
    source.start();

    std::this_thread::sleep_for(std::chrono::seconds(5));

    source.stop();
    queue.close();
    processor.stop();

    std::cout << "Final processed count: " << processor.getProcessedCount() << std::endl;

    return 0;
}
