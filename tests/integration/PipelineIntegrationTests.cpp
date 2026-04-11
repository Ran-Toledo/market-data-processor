#include "processing/EventProcessor.h"

#include <cassert>

namespace
{
    mdp::MarketDataEvent makeEvent(
        mdp::SequenceNumber sequenceNumber,
        double price,
        std::uint32_t volume)
    {
        mdp::MarketDataEvent event;
        event.symbol = "AAPL";
        event.price = price;
        event.volume = volume;
        event.exchangeTimestampNs = 1;
        event.ingestTimestampNs = 1;
        event.sequenceNumber = sequenceNumber;
        return event;
    }
}

void runPipelineIntegrationTests()
{
    mdp::EventProcessor processor;

    const auto first = processor.process(makeEvent(1, 100.0, 10));
    const auto second = processor.process(makeEvent(2, 105.0, 20));

    assert(first.processed);
    assert(second.processed);

    const auto stateSnapshot = processor.getStateSnapshot();
    const auto stateIt = stateSnapshot.find("AAPL");
    assert(stateIt != stateSnapshot.end());
    assert(stateIt->second.lastPrice == 105.0);
    assert(stateIt->second.lastVolume == 20);
    assert(stateIt->second.lastSequenceNumber == 2);

    const auto statsSnapshot = processor.getStatsSnapshot();
    const auto statsIt = statsSnapshot.find("AAPL");
    assert(statsIt != statsSnapshot.end());
    assert(statsIt->second.eventCount == 2);
    assert(statsIt->second.totalVolume == 30);
    assert(statsIt->second.minPrice == 100.0);
    assert(statsIt->second.maxPrice == 105.0);
}
