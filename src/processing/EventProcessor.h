#pragma once

#include "core/MarketDataEvent.h"
#include "processing/RiskRuleEvaluator.h"
#include "processing/SequenceTracker.h"
#include "processing/SymbolStateStore.h"
#include "processing/SymbolStats.h"

#include <atomic>
#include <cstdint>
#include <vector>

namespace mdp
{
    class EventProcessor
    {
    public:
        EventProcessor(
            SymbolStateStore& stateStore,
            SymbolStats& symbolStats);

        void process(const MarketDataEvent& event);

        std::uint64_t getProcessedCount() const;
        std::uint64_t getValidCount() const;
        std::uint64_t getInvalidCount() const;
        std::uint64_t getDuplicateCount() const;
        std::uint64_t getOutOfOrderCount() const;
        std::uint64_t getAlertCount() const;

        std::uint64_t getAverageLatencyNs() const;
        std::uint64_t getMinLatencyNs() const;
        std::uint64_t getMaxLatencyNs() const;

    private:
        void updateLatencyMetrics(std::uint64_t latencyNs);
        void logAlerts(const std::vector<RuleAlert>& alerts) const;

    private:
        SymbolStateStore& m_stateStore;
        SymbolStats& m_symbolStats;
        SequenceTracker m_sequenceTracker;
        RiskRuleEvaluator m_riskRuleEvaluator;

        std::atomic<std::uint64_t> m_processedCount{ 0 };
        std::atomic<std::uint64_t> m_validCount{ 0 };
        std::atomic<std::uint64_t> m_invalidCount{ 0 };
        std::atomic<std::uint64_t> m_duplicateCount{ 0 };
        std::atomic<std::uint64_t> m_outOfOrderCount{ 0 };
        std::atomic<std::uint64_t> m_alertCount{ 0 };

        std::atomic<std::uint64_t> m_totalLatencyNs{ 0 };
        std::atomic<std::uint64_t> m_minLatencyNs{ UINT64_MAX };
        std::atomic<std::uint64_t> m_maxLatencyNs{ 0 };
    };
}
