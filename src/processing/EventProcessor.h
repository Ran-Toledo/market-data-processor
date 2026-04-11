#pragma once

#include "metrics/LatencyRecorder.h"
#include "metrics/MetricsCollector.h"
#include "output/IEventSink.h"
#include "processing/EventProcessingResult.h"
#include "processing/RiskRuleEvaluator.h"
#include "processing/SequenceTracker.h"
#include "processing/SymbolStateStore.h"
#include "processing/SymbolStats.h"

namespace mdp
{
    class EventProcessor
    {
    public:
        EventProcessor() = default;
        explicit EventProcessor(IEventSink* eventSink);

        EventProcessingResult process(const MarketDataEvent& event);

        const MetricsCollector& getMetrics() const { return m_metrics; }
        const LatencyRecorder& getLatency() const { return m_latency; }
        std::unordered_map<Symbol, SymbolState> getStateSnapshot() const;
        std::unordered_map<Symbol, SymbolStatistics> getStatsSnapshot() const;
        std::size_t getTrackedStateSymbolCount() const;
        std::size_t getTrackedStatsSymbolCount() const;

    private:
        void simulateProcessingLoad() const;
        void publishAlerts(const std::vector<RuleAlert>& alerts) const;
        void publishStateChange(const StateChange& stateChange) const;

    private:
        SymbolStateStore m_stateStore;
        SymbolStats m_symbolStats;

        SequenceTracker m_sequenceTracker;
        RiskRuleEvaluator m_riskRuleEvaluator;
        IEventSink* m_eventSink{ nullptr };

        MetricsCollector m_metrics;
        LatencyRecorder m_latency;
    };
}
