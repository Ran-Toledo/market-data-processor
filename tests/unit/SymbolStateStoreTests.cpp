#include "api/domain/MarketDataEvent.h"
#include "processing/SymbolStateStore.h"

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
        event.exchangeTimestampNs = sequenceNumber * 10;
        event.ingestTimestampNs = sequenceNumber * 20;
        event.sequenceNumber = sequenceNumber;
        return event;
    }
}

void runSymbolStateStoreTests()
{
    mdp::processing::SymbolStateStore store;

    assert(!store.tryGet("AAPL").has_value());
    assert(store.getTrackedSymbolCount() == 0);

    store.update(makeEvent(1, 100.0, 10));

    const auto firstState = store.tryGet("AAPL");
    assert(firstState.has_value());
    assert(firstState->lastPrice == 100.0);
    assert(firstState->lastVolume == 10);
    assert(firstState->lastSequenceNumber == 1);
    assert(firstState->lastExchangeTimestampNs == 10);
    assert(firstState->lastIngestTimestampNs == 20);
    assert(store.getTrackedSymbolCount() == 1);

    store.update(makeEvent(2, 105.0, 20));

    const auto updatedState = store.tryGet("AAPL");
    assert(updatedState.has_value());
    assert(updatedState->lastPrice == 105.0);
    assert(updatedState->lastVolume == 20);
    assert(updatedState->lastSequenceNumber == 2);
    assert(updatedState->lastExchangeTimestampNs == 20);
    assert(updatedState->lastIngestTimestampNs == 40);

    const auto snapshot = store.snapshot();
    assert(snapshot.size() == 1);
    assert(snapshot.at("AAPL").lastPrice == 105.0);
}
