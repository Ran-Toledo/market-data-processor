// EventProcessor.h
#pragma once

#include "core/MarketDataEvent.h"
#include "core/ThreadSafeQueue.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
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
        std::uint64_t getTotalLatencyNs() const;
        double getAverageLatencyNs() const;
        std::uint64_t getMinLatencyNs() const;
        std::uint64_t getMaxLatencyNs() const;

    private:
        void processLoop();

    private:
        ThreadSafeQueue<MarketDataEvent>& m_queue;
        std::thread m_workerThread;
        std::atomic<bool> m_running = false;
        std::atomic<std::size_t> m_processedCount = 0;
        std::atomic<std::uint64_t> m_totalLatencyNs = 0;
        std::atomic<std::uint64_t> m_minLatencyNs = UINT64_MAX;
        std::atomic<std::uint64_t> m_maxLatencyNs = 0;
    };
}
