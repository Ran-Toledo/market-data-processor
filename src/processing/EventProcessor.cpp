#include "processing/EventProcessor.h"

#include "config/AppConfig.h"
#include "processing/EventValidator.h"
#include "util/Clock.h"

#include <chrono>
#include <thread>
using namespace mdp::processing::validation;

namespace mdp::processing
{
    namespace
    {
        SymbolState makeState(const MarketDataEvent& event)
        {
            SymbolState state;
            state.lastPrice = event.price;
            state.lastVolume = event.volume;
            state.lastExchangeTimestampNs = event.exchangeTimestampNs;
            state.lastIngestTimestampNs = event.ingestTimestampNs;
            state.lastSequenceNumber = event.sequenceNumber;
            return state;
        }
    }

    EventProcessor::EventProcessor(output::IEventSink* eventSink)
        : m_eventSink(eventSink)
    {
    }

    EventProcessingResult EventProcessor::process(const MarketDataEvent& event)
    {
        const auto processingStartNs = clock::nowNs();
        if (event.enqueueTimestampNs > 0 && processingStartNs >= event.enqueueTimestampNs)
        {
            m_queueWaitLatency.record(processingStartNs - event.enqueueTimestampNs);
        }

        simulateProcessingLoad();

        EventProcessingResult result;

        const ValidationResult validationResult = validate(event);
        result.validation = validationResult;
        if (!validationResult.isValid)
        {
            m_metrics.onInvalid();
            return result;
        }

        m_metrics.onValid();

        result.sequence = m_sequenceTracker.evaluate(event.symbol, event.sequenceNumber);

        if (result.sequence.status == SequenceStatus::Duplicate)
        {
            m_metrics.onDuplicate();
            return result;
        }

        if (result.sequence.status == SequenceStatus::OutOfOrder)
        {
            m_metrics.onOutOfOrder();
            return result;
        }

        if (result.sequence.status == SequenceStatus::Gap)
        {
            m_metrics.onSequenceGap();
        }

        const auto prev = m_stateStore.tryGet(event.symbol);
        const auto alerts = m_riskRuleEvaluator.evaluate(event, prev);
        result.alerts = alerts;

        StateChange stateChange;
        stateChange.symbol = event.symbol;
        stateChange.previousState = prev;
        stateChange.currentState = makeState(event);

        m_stateStore.update(event);
        m_symbolStats.record(event);
        result.stateChange = stateChange;

        const auto now = clock::nowNs();
        const auto latency =
            now >= event.ingestTimestampNs ? now - event.ingestTimestampNs : 0;
        result.latencyNs = latency;

        m_latency.record(latency);

        if (!alerts.empty())
        {
            m_metrics.onAlerts(alerts.size());
            publishAlerts(alerts);
        }

        publishStateChange(stateChange);
        m_metrics.onProcessed();
        result.processed = true;
        publishProcessedEvent(event);
        return result;
    }

    std::unordered_map<Symbol, SymbolState> EventProcessor::getStateSnapshot() const
    {
        return m_stateStore.snapshot();
    }

    std::unordered_map<Symbol, SymbolStatistics> EventProcessor::getStatsSnapshot() const
    {
        return m_symbolStats.snapshot();
    }

    std::size_t EventProcessor::getTrackedStateSymbolCount() const
    {
        return m_stateStore.getTrackedSymbolCount();
    }

    std::size_t EventProcessor::getTrackedStatsSymbolCount() const
    {
        return m_symbolStats.getTrackedSymbolCount();
    }

    void EventProcessor::simulateProcessingLoad() const
    {
        if (config::get().worker().processingDelayUs > 0)
        {
            std::this_thread::sleep_for(
                std::chrono::microseconds(config::get().worker().processingDelayUs));
        }

        volatile std::uint64_t sink = 0;

        for (std::size_t i = 0;
            i < config::get().worker().optionalBusyWorkIterations;
            ++i)
        {
            sink += static_cast<std::uint64_t>(i) * 1664525ULL + 1013904223ULL;
        }
    }

    void EventProcessor::publishProcessedEvent(const MarketDataEvent& event) const
    {
        if (m_eventSink != nullptr)
        {
            m_eventSink->publishProcessedEvent(event);
        }
    }

    void EventProcessor::publishAlerts(const std::vector<RuleAlert>& alerts) const
    {
        if (m_eventSink != nullptr && config::get().logging().enableAlertLogging)
        {
            for (const RuleAlert& alert : alerts)
            {
                m_eventSink->publishAlert(alert);
            }
        }
    }

    void EventProcessor::publishStateChange(const StateChange& stateChange) const
    {
        if (m_eventSink != nullptr && config::get().logging().enableEventLogging)
        {
            m_eventSink->publishStateChange(stateChange);
        }
    }
}
