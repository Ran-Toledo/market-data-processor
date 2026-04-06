#pragma once

#include "metrics/LatencyRecorder.h"
#include "metrics/MetricsCollector.h"
#include "processing/RiskRuleEvaluator.h"
#include "processing/SequenceTracker.h"
#include "processing/SymbolStateStore.h"
#include "processing/SymbolStats.h"

namespace mdp
{
    class EventProcessor
    {
    public:
        EventProcessor(SymbolStateStore& stateStore, SymbolStats& symbolStats);

        void process(const MarketDataEvent& event);

        const MetricsCollector& getMetrics() const { return m_metrics; }
        const LatencyRecorder& getLatency() const { return m_latency; }

    private:
        void simulateProcessingLoad() const;
        void logAlerts(const std::vector<RuleAlert>& alerts) const;

    private:
        SymbolStateStore& m_stateStore;
        SymbolStats& m_symbolStats;

        SequenceTracker m_sequenceTracker;
        RiskRuleEvaluator m_riskRuleEvaluator;

        MetricsCollector m_metrics;
        LatencyRecorder m_latency;
    };
}
