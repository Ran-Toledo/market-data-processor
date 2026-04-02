#pragma once

#include "core/MarketDataEvent.h"
#include "core/ThreadSafeQueue.h"

#include <atomic>
#include <cstddef>
#include <thread>

namespace mdp
{
    class EventProcessor
    {
    public:
        explicit EventProcessor(ThreadSafeQueue<MarketDataEvent>& queue);
        ~EventProcessor();

        void start();
        void stop();

        std::size_t getProcessedCount() const;

    private:
        void processLoop();

    private:
        ThreadSafeQueue<MarketDataEvent>& m_queue;
        std::thread m_workerThread;
        std::atomic<bool> m_running = false;
        std::atomic<std::size_t> m_processedCount = 0;
    };
}
