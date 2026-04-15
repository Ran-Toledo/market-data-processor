#include "output/IEventSink.h"
#include "processing/EventProcessor.h"

#include <cassert>
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

    class RecordingSink : public mdp::output::IEventSink
    {
    public:
        void publishProcessedEvent(const mdp::MarketDataEvent& event) override
        {
            processedEvents.push_back(event);
        }

        void publishAlert(const mdp::processing::RuleAlert& alert) override
        {
            alerts.push_back(alert);
        }

        void publishStateChange(const mdp::processing::StateChange& stateChange) override
        {
            stateChanges.push_back(stateChange);
        }

        std::vector<mdp::MarketDataEvent> processedEvents;
        std::vector<mdp::processing::RuleAlert> alerts;
        std::vector<mdp::processing::StateChange> stateChanges;
    };

    void testEventProcessorSequencePolicy()
    {
        RecordingSink sink;
        mdp::processing::EventProcessor processor(&sink);

        const auto first = processor.process(makeEvent(1));
        assert(first.processed);
        assert(first.sequence.status == mdp::processing::SequenceStatus::New);
        assert(processor.getMetrics().getProcessed() == 1);
        assert(processor.getTrackedStateSymbolCount() == 1);
        assert(processor.getTrackedStatsSymbolCount() == 1);
        assert(sink.stateChanges.size() == 1);

        const auto duplicate = processor.process(makeEvent(1, 101.0));
        assert(!duplicate.processed);
        assert(duplicate.sequence.status == mdp::processing::SequenceStatus::Duplicate);
        assert(processor.getMetrics().getDuplicate() == 1);
        assert(processor.getMetrics().getProcessed() == 1);

        const auto gap = processor.process(makeEvent(3, 102.0));
        assert(gap.processed);
        assert(gap.sequence.status == mdp::processing::SequenceStatus::Gap);
        assert(processor.getMetrics().getSequenceGap() == 1);
        assert(processor.getMetrics().getProcessed() == 2);

        const auto outOfOrder = processor.process(makeEvent(2, 103.0));
        assert(!outOfOrder.processed);
        assert(outOfOrder.sequence.status == mdp::processing::SequenceStatus::OutOfOrder);
        assert(processor.getMetrics().getOutOfOrder() == 1);
        assert(processor.getMetrics().getProcessed() == 2);
    }

    void testEventProcessorAlerts()
    {
        RecordingSink sink;
        mdp::processing::EventProcessor processor(&sink);

        const auto first = processor.process(makeEvent(1, 100.0, 100));
        assert(first.processed);

        const auto priceJump = processor.process(makeEvent(2, 110.0, 100));
        assert(priceJump.processed);
        assert(priceJump.alerts.size() == 1);
        assert(priceJump.alerts[0].type == mdp::processing::RuleType::PriceJump);

        const auto largeVolume = processor.process(makeEvent(3, 111.0, 10000));
        assert(largeVolume.processed);
        assert(largeVolume.alerts.size() == 1);
        assert(largeVolume.alerts[0].type == mdp::processing::RuleType::LargeVolume);

        assert(sink.alerts.size() == 2);
    }
}

void runEventProcessorTests()
{
    testEventProcessorSequencePolicy();
    testEventProcessorAlerts();
}
