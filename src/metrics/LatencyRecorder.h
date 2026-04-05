#pragma once

#include <atomic>
#include <cstdint>

namespace mdp
{
    class LatencyRecorder
    {
    public:
        void record(std::uint64_t latencyNs);

        std::uint64_t getAverageLatencyNs() const;
        std::uint64_t getMinLatencyNs() const;
        std::uint64_t getMaxLatencyNs() const;
        std::uint64_t getCount() const;

    private:
        std::atomic<std::uint64_t> m_totalLatencyNs{ 0 };
        std::atomic<std::uint64_t> m_minLatencyNs{ UINT64_MAX };
        std::atomic<std::uint64_t> m_maxLatencyNs{ 0 };
        std::atomic<std::uint64_t> m_count{ 0 };
    };
}
