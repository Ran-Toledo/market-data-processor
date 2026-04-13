#include "metrics/LatencyRecorder.h"

#include <algorithm>
#include <cmath>

namespace mdp::metrics
{
    void LatencyRecorder::record(std::uint64_t latencyNs)
    {
        m_totalLatencyNs.fetch_add(latencyNs);
        m_count.fetch_add(1);
        m_buckets[getBucketIndex(latencyNs)].fetch_add(1);

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

    std::uint64_t LatencyRecorder::getPercentileLatencyNs(double percentile) const
    {
        return percentileFromBuckets(
            getBucketSnapshot(),
            getCount(),
            percentile);
    }

    LatencyRecorder::BucketSnapshot LatencyRecorder::getBucketSnapshot() const
    {
        BucketSnapshot snapshot{};

        for (std::size_t i = 0; i < m_buckets.size(); ++i)
        {
            snapshot[i] = m_buckets[i].load();
        }

        return snapshot;
    }

    std::uint64_t LatencyRecorder::percentileFromBuckets(
        const BucketSnapshot& buckets,
        std::uint64_t count,
        double percentile)
    {
        if (count == 0)
        {
            return 0;
        }

        percentile = std::clamp(percentile, 0.0, 100.0);
        const auto targetRank = static_cast<std::uint64_t>(
            std::ceil((percentile / 100.0) * static_cast<double>(count)));
        const std::uint64_t clampedRank = std::max<std::uint64_t>(1, targetRank);

        std::uint64_t runningCount = 0;

        for (std::size_t i = 0; i < buckets.size(); ++i)
        {
            runningCount += buckets[i];

            if (runningCount >= clampedRank)
            {
                return getBucketUpperBoundNs(i);
            }
        }

        return getBucketUpperBoundNs(buckets.size() - 1);
    }

    std::size_t LatencyRecorder::getBucketIndex(std::uint64_t latencyNs)
    {
        if (latencyNs <= 1)
        {
            return 0;
        }

        std::size_t bucketIndex = 0;
        std::uint64_t upperBound = 1;

        while (upperBound < latencyNs &&
            bucketIndex + 1 < kLatencyBucketCount)
        {
            upperBound <<= 1;
            ++bucketIndex;
        }

        return bucketIndex;
    }

    std::uint64_t LatencyRecorder::getBucketUpperBoundNs(std::size_t bucketIndex)
    {
        if (bucketIndex >= kLatencyBucketCount - 1)
        {
            return UINT64_MAX;
        }

        return std::uint64_t{ 1 } << bucketIndex;
    }
}
