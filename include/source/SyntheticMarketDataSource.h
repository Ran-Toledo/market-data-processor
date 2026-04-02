// SyntheticMarketDataSource.h
#pragma once

#include "core/MarketDataEvent.h"
#include "core/ThreadSafeQueue.h"

#include <atomic>
#include <thread>
#include <vector>

namespace mdp
{
    class SyntheticMarketDataSource
    {
    public:
        explicit SyntheticMarketDataSource(ThreadSafeQueue<MarketDataEvent>& queue);
        ~SyntheticMarketDataSource();

        void start();
        void stop();

    private:
        void generateLoop();
        MarketDataEvent generateEvent();

    private:
        ThreadSafeQueue<MarketDataEvent>& m_queue;
        std::thread m_workerThread;
        std::atomic<bool> m_running = false;
        SequenceNumber m_nextSequenceNumber = 1;
        std::vector<Symbol> m_symbols;
    };
}
