#include "processing/EventProcessor.h"

#include "core/AppConfig.h"
#include "processing/EventValidator.h"
#include "util/Clock.h"

#include <iostream>

using namespace mdp::config;
using namespace mdp::validation;

namespace mdp
{
    EventProcessor::EventProcessor(SymbolStateStore& stateStore, SymbolStats& symbolStats)
        : m_stateStore(stateStore)
        , m_symbolStats(symbolStats)
    {
    }

    void EventProcessor::process(const MarketDataEvent& event)
    {
        const ValidationResult validationResult = validate(event);
        if (!validationResult.isValid)
        {
            m_metrics.onInvalid();
            return;
        }

        m_metrics.onValid();

        const auto seqStatus =
            m_sequenceTracker.evaluate(event.symbol, event.sequenceNumber);

        if (seqStatus == SequenceStatus::Duplicate)
        {
            m_metrics.onDuplicate();
            return;
        }

        if (seqStatus == SequenceStatus::OutOfOrder)
        {
            m_metrics.onOutOfOrder();
            return;
        }

        const auto prev = m_stateStore.tryGet(event.symbol);
        const auto alerts = m_riskRuleEvaluator.evaluate(event, prev);

        m_stateStore.update(event);
        m_symbolStats.record(event);

        const auto now = clock::nowNs();
        const auto latency =
            now >= event.ingestTimestampNs ? now - event.ingestTimestampNs : 0;

        m_latency.record(latency);

        if (!alerts.empty())
        {
            m_metrics.onAlerts(alerts.size());
            logAlerts(alerts);
        }

        m_metrics.onProcessed();
    }

    void EventProcessor::logAlerts(const std::vector<RuleAlert>& alerts) const
    {
        if (enableAlertLogging)
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
    }
}

