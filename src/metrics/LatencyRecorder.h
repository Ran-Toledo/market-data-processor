#pragma once

#include <array>
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
        std::uint64_t getPercentileLatencyNs(double percentile) const;

        static constexpr std::size_t kLatencyBucketCount = 64;
        using BucketSnapshot = std::array<std::uint64_t, kLatencyBucketCount>;

        BucketSnapshot getBucketSnapshot() const;
        static std::uint64_t percentileFromBuckets(
            const BucketSnapshot& buckets,
            std::uint64_t count,
            double percentile);

    private:
        static std::size_t getBucketIndex(std::uint64_t latencyNs);
        static std::uint64_t getBucketUpperBoundNs(std::size_t bucketIndex);

    private:
        std::atomic<std::uint64_t> m_totalLatencyNs{ 0 };
        std::atomic<std::uint64_t> m_minLatencyNs{ UINT64_MAX };
        std::atomic<std::uint64_t> m_maxLatencyNs{ 0 };
        std::atomic<std::uint64_t> m_count{ 0 };
        std::array<std::atomic<std::uint64_t>, kLatencyBucketCount> m_buckets{};
    };
}
