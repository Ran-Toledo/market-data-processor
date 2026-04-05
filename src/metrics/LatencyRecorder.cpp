#include "metrics/LatencyRecorder.h"

namespace mdp
{
    void LatencyRecorder::record(std::uint64_t latencyNs)
    {
        m_totalLatencyNs.fetch_add(latencyNs);
        m_count.fetch_add(1);

        std::uint64_t currentMin = m_minLatencyNs.load();
        while (latencyNs < currentMin &&
            !m_minLatencyNs.compare_exchange_weak(currentMin, latencyNs))
        {
        }

        std::uint64_t currentMax = m_maxLatencyNs.load();
        while (latencyNs > currentMax &&
            !m_maxLatencyNs.compare_exchange_weak(currentMax, latencyNs))
        {
        }
    }

    std::uint64_t LatencyRecorder::getAverageLatencyNs() const
    {
        const auto count = m_count.load();
        if (count == 0)
        {
            return 0;
        }

        return m_totalLatencyNs.load() / count;
    }

    std::uint64_t LatencyRecorder::getMinLatencyNs() const
    {
        const auto minVal = m_minLatencyNs.load();
        return minVal == UINT64_MAX ? 0 : minVal;
    }

    std::uint64_t LatencyRecorder::getMaxLatencyNs() const
    {
        return m_maxLatencyNs.load();
    }

    std::uint64_t LatencyRecorder::getCount() const
    {
        return m_count.load();
    }
}
