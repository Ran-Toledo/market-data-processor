// WorkerPool.h
#pragma once

#include "core/MarketDataEvent.h"
#include "processing/EventProcessor.h"
#include "queue/ThreadSafeQueue.h"

#include <atomic>
#include <cstddef>
#include <thread>
#include <vector>

namespace mdp
{
    class WorkerPool
    {
    public:
        WorkerPool(
            ThreadSafeQueue<MarketDataEvent>& queue,
            EventProcessor& processor,
            std::size_t workerCount);
        ~WorkerPool();

        void start();
        void stop();

    private:
        void workerLoop();

    private:
        ThreadSafeQueue<MarketDataEvent>& m_queue;
        EventProcessor& m_processor;
        std::size_t m_workerCount;
        std::vector<std::thread> m_workerThreads;
        std::atomic<bool> m_running = false;
    };
}
