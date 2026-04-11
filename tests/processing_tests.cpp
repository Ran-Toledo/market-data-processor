#include "output/IEventSink.h"
#include "processing/EventProcessor.h"
#include "processing/SequenceTracker.h"
#include "processing/SymbolStats.h"

#include <cassert>
#include <unordered_map>
#include <vector>

namespace
{
    mdp::MarketDataEvent makeEvent(
        mdp::SequenceNumber sequenceNumber,
        double price = 100.0,
        std::uint32_t volume = 100,
        const mdp::Symbol& symbol = "AAPL")
    {
        mdp::MarketDataEvent event;
        event.symbol = symbol;
        event.price = price;
        event.volume = volume;
        event.exchangeTimestampNs = 1;
        event.ingestTimestampNs = 1;
        event.sequenceNumber = sequenceNumber;
        return event;
    }

    class RecordingSink : public mdp::IEventSink
    {
    public:
        void publishProcessedEvent(const mdp::MarketDataEvent& event) override
        {
            processedEvents.push_back(event);
        }

        void publishAlert(const mdp::RuleAlert& alert) override
        {
            alerts.push_back(alert);
        }

        void publishStateChange(const mdp::StateChange& stateChange) override
        {
            stateChanges.push_back(stateChange);
        }

        std::vector<mdp::MarketDataEvent> processedEvents;
        std::vector<mdp::RuleAlert> alerts;
        std::vector<mdp::StateChange> stateChanges;
    };

    void testSequenceTracker()
    {
        mdp::SequenceTracker tracker;

        const auto first = tracker.evaluate("AAPL", 1);
        assert(first.status == mdp::SequenceStatus::New);
        assert(first.shouldProcess());

        const auto duplicate = tracker.evaluate("AAPL", 1);
        assert(duplicate.status == mdp::SequenceStatus::Duplicate);
        assert(!duplicate.shouldProcess());
        assert(duplicate.previousSequenceNumber == 1);
        assert(duplicate.expectedSequenceNumber == 2);

        const auto gap = tracker.evaluate("AAPL", 3);
        assert(gap.status == mdp::SequenceStatus::Gap);
        assert(gap.shouldProcess());
        assert(gap.previousSequenceNumber == 1);
        assert(gap.expectedSequenceNumber == 2);

        const auto outOfOrder = tracker.evaluate("AAPL", 2);
        assert(outOfOrder.status == mdp::SequenceStatus::OutOfOrder);
        assert(!outOfOrder.shouldProcess());

        const auto contiguous = tracker.evaluate("AAPL", 4);
        assert(contiguous.status == mdp::SequenceStatus::New);
        assert(contiguous.shouldProcess());
    }

    void testEventProcessorSequencePolicy()
    {
        RecordingSink sink;
        mdp::EventProcessor processor(&sink);

        const auto first = processor.process(makeEvent(1));
        assert(first.processed);
        assert(first.sequence.status == mdp::SequenceStatus::New);
        assert(processor.getMetrics().getProcessed() == 1);
        assert(processor.getTrackedStateSymbolCount() == 1);
        assert(processor.getTrackedStatsSymbolCount() == 1);
        assert(sink.stateChanges.size() == 1);

        const auto duplicate = processor.process(makeEvent(1, 101.0));
        assert(!duplicate.processed);
        assert(duplicate.sequence.status == mdp::SequenceStatus::Duplicate);
        assert(processor.getMetrics().getDuplicate() == 1);
        assert(processor.getMetrics().getProcessed() == 1);

        const auto gap = processor.process(makeEvent(3, 102.0));
        assert(gap.processed);
        assert(gap.sequence.status == mdp::SequenceStatus::Gap);
        assert(processor.getMetrics().getSequenceGap() == 1);
        assert(processor.getMetrics().getProcessed() == 2);

        const auto outOfOrder = processor.process(makeEvent(2, 103.0));
        assert(!outOfOrder.processed);
        assert(outOfOrder.sequence.status == mdp::SequenceStatus::OutOfOrder);
        assert(processor.getMetrics().getOutOfOrder() == 1);
        assert(processor.getMetrics().getProcessed() == 2);
    }

    void testEventProcessorAlerts()
    {
        RecordingSink sink;
        mdp::EventProcessor processor(&sink);

        const auto first = processor.process(makeEvent(1, 100.0, 100));
        assert(first.processed);

        const auto priceJump = processor.process(makeEvent(2, 110.0, 100));
        assert(priceJump.processed);
        assert(priceJump.alerts.size() == 1);
        assert(priceJump.alerts[0].type == mdp::RuleType::PriceJump);

        const auto largeVolume = processor.process(makeEvent(3, 111.0, 10000));
        assert(largeVolume.processed);
        assert(largeVolume.alerts.size() == 1);
        assert(largeVolume.alerts[0].type == mdp::RuleType::LargeVolume);

        assert(sink.alerts.size() == 2);
    }

    void testSymbolStatsAggregation()
    {
        mdp::SymbolStats stats;
        stats.record(makeEvent(1, 10.0, 100, "AAPL"));
        stats.record(makeEvent(2, 20.0, 200, "AAPL"));

        const auto snapshot = stats.snapshot();
        const auto it = snapshot.find("AAPL");
        assert(it != snapshot.end());
        assert(it->second.eventCount == 2);
        assert(it->second.totalVolume == 300);
        assert(it->second.minPrice == 10.0);
        assert(it->second.maxPrice == 20.0);
        assert(it->second.averagePrice == 15.0);

        std::unordered_map<mdp::Symbol, mdp::SymbolStatistics> merged;
        mdp::SymbolStats::mergeInto(merged, "AAPL", it->second);
        mdp::SymbolStats::mergeInto(merged, "AAPL", it->second);

        assert(merged["AAPL"].eventCount == 4);
        assert(merged["AAPL"].totalVolume == 600);
        assert(merged["AAPL"].minPrice == 10.0);
        assert(merged["AAPL"].maxPrice == 20.0);
        assert(merged["AAPL"].averagePrice == 15.0);
    }
}

int main()
{
    testSequenceTracker();
    testEventProcessorSequencePolicy();
    testEventProcessorAlerts();
    testSymbolStatsAggregation();
    return 0;
}
