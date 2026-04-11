#pragma once

#include <atomic>
#include <cstdint>

namespace mdp
{
    class MetricsCollector
    {
    public:
        void onValid() { m_valid.fetch_add(1); }
        void onInvalid() { m_invalid.fetch_add(1); }
        void onDuplicate() { m_duplicate.fetch_add(1); }
        void onOutOfOrder() { m_outOfOrder.fetch_add(1); }
        void onSequenceGap() { m_sequenceGap.fetch_add(1); }
        void onProcessed() { m_processed.fetch_add(1); }
        void onAlerts(std::uint64_t count) { m_alerts.fetch_add(count); }

        std::uint64_t getProcessed() const { return m_processed.load(); }
        std::uint64_t getValid() const { return m_valid.load(); }
        std::uint64_t getInvalid() const { return m_invalid.load(); }
        std::uint64_t getDuplicate() const { return m_duplicate.load(); }
        std::uint64_t getOutOfOrder() const { return m_outOfOrder.load(); }
        std::uint64_t getSequenceGap() const { return m_sequenceGap.load(); }
        std::uint64_t getAlerts() const { return m_alerts.load(); }

    private:
        std::atomic<std::uint64_t> m_processed{ 0 };
        std::atomic<std::uint64_t> m_valid{ 0 };
        std::atomic<std::uint64_t> m_invalid{ 0 };
        std::atomic<std::uint64_t> m_duplicate{ 0 };
        std::atomic<std::uint64_t> m_outOfOrder{ 0 };
        std::atomic<std::uint64_t> m_sequenceGap{ 0 };
        std::atomic<std::uint64_t> m_alerts{ 0 };
    };
}
