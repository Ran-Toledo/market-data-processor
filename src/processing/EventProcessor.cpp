#include "processing/EventProcessor.h"

#include "core/AppConfig.h"
#include "processing/EventValidator.h"
#include "util/Clock.h"

#include <iostream>

using namespace mdp::config;
using namespace mdp::validation;

namespace mdp
{
    EventProcessor::EventProcessor(
        SymbolStateStore& stateStore,
        SymbolStats& symbolStats)
        : m_stateStore(stateStore)
        , m_symbolStats(symbolStats)
    {
    }

    void EventProcessor::process(const MarketDataEvent& event)
    {
        const ValidationResult validationResult = validation::validate(event);
        if (!validationResult.isValid)
        {
            m_invalidCount.fetch_add(1);

            if (enableEventLogging)
            {
                std::cout << "Rejected invalid event | " << event << std::endl;
            }

            return;
        }

        m_validCount.fetch_add(1);

        const SequenceStatus sequenceStatus =
            m_sequenceTracker.evaluate(event.symbol, event.sequenceNumber);

        if (sequenceStatus == SequenceStatus::Duplicate)
        {
            m_duplicateCount.fetch_add(1);

            if (enableEventLogging)
            {
                std::cout << "Dropped duplicate event | " << event << std::endl;
            }

            return;
        }

        if (sequenceStatus == SequenceStatus::OutOfOrder)
        {
            m_outOfOrderCount.fetch_add(1);

            if (enableEventLogging)
            {
                std::cout << "Dropped out-of-order event | " << event << std::endl;
            }

            return;
        }

        const std::optional<SymbolState> previousState = m_stateStore.tryGet(event.symbol);
        const std::vector<RuleAlert> alerts = m_riskRuleEvaluator.evaluate(event, previousState);

        m_stateStore.update(event);
        m_symbolStats.record(event);

        const TimestampNs nowNs = clock::nowNs();
        const std::uint64_t latencyNs =
            nowNs >= event.ingestTimestampNs ? nowNs - event.ingestTimestampNs : 0;

        const std::uint64_t processedCount = m_processedCount.fetch_add(1) + 1;

        updateLatencyMetrics(latencyNs);

        if (!alerts.empty())
        {
            m_alertCount.fetch_add(static_cast<std::uint64_t>(alerts.size()));
            logAlerts(alerts);
        }

        if (enableEventLogging)
        {
            std::cout << "Processed event | " << event
                << " | latency(ns): " << latencyNs << std::endl;
        }

        if (enableProcessingStatsLogging &&
            processingStatsLogInterval > 0 &&
            (processedCount % processingStatsLogInterval == 0))
        {
            std::cout << "Processed events: " << processedCount
                << " | valid: " << getValidCount()
                << " | invalid: " << getInvalidCount()
                << " | duplicates: " << getDuplicateCount()
                << " | out-of-order: " << getOutOfOrderCount()
                << " | alerts: " << getAlertCount()
                << " | avg latency(ns): " << getAverageLatencyNs()
                << " | min latency(ns): " << getMinLatencyNs()
                << " | max latency(ns): " << getMaxLatencyNs()
                << std::endl;
        }
    }

    void EventProcessor::updateLatencyMetrics(std::uint64_t latencyNs)
    {
        m_totalLatencyNs.fetch_add(latencyNs);

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

    void EventProcessor::logAlerts(const std::vector<RuleAlert>& alerts) const
    {
        for (const RuleAlert& alert : alerts)
        {
            std::cout << "Rule alert | symbol: " << alert.symbol
                << " | type: ";

            switch (alert.type)
            {
            case RuleType::PriceJump:
                std::cout << "PriceJump";
                break;
            case RuleType::LargeVolume:
                std::cout << "LargeVolume";
                break;
            default:
                std::cout << "Unknown";
                break;
            }

            std::cout << " | message: " << alert.message << std::endl;
        }
    }

    std::uint64_t EventProcessor::getProcessedCount() const
    {
        return m_processedCount.load();
    }

    std::uint64_t EventProcessor::getValidCount() const
    {
        return m_validCount.load();
    }

    std::uint64_t EventProcessor::getInvalidCount() const
    {
        return m_invalidCount.load();
    }

    std::uint64_t EventProcessor::getDuplicateCount() const
    {
        return m_duplicateCount.load();
    }

    std::uint64_t EventProcessor::getOutOfOrderCount() const
    {
        return m_outOfOrderCount.load();
    }

    std::uint64_t EventProcessor::getAlertCount() const
    {
        return m_alertCount.load();
    }

    std::uint64_t EventProcessor::getAverageLatencyNs() const
    {
        const std::uint64_t processedCount = m_processedCount.load();
        if (processedCount == 0)
        {
            return 0;
        }

        return m_totalLatencyNs.load() / processedCount;
    }

    std::uint64_t EventProcessor::getMinLatencyNs() const
    {
        const std::uint64_t minLatencyNs = m_minLatencyNs.load();
        return minLatencyNs == UINT64_MAX ? 0 : minLatencyNs;
    }

    std::uint64_t EventProcessor::getMaxLatencyNs() const
    {
        return m_maxLatencyNs.load();
    }
}
