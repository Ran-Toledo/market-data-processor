#include "processing/SymbolStats.h"

#include <cassert>
#include <unordered_map>

namespace
{
    mdp::MarketDataEvent makeEvent(
        mdp::SequenceNumber sequenceNumber,
        double price,
        std::uint32_t volume,
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
}

void runSymbolStatsTests()
{
    mdp::processing::SymbolStats stats;
    stats.record(makeEvent(1, 10.0, 100));
    stats.record(makeEvent(2, 20.0, 200));
    stats.record(makeEvent(3, 15.0, 300));

    const auto snapshot = stats.snapshot();
    const auto it = snapshot.find("AAPL");
    assert(it != snapshot.end());
    assert(it->second.eventCount == 3);
    assert(it->second.totalVolume == 600);
    assert(it->second.minPrice == 10.0);
    assert(it->second.maxPrice == 20.0);
    assert(it->second.lastPrice == 15.0);
    assert(it->second.averagePrice == 15.0);

    std::unordered_map<mdp::Symbol, mdp::processing::SymbolStatistics> merged;
    mdp::processing::SymbolStats::mergeInto(merged, "AAPL", it->second);
    mdp::processing::SymbolStats::mergeInto(merged, "AAPL", it->second);

    assert(merged["AAPL"].eventCount == 6);
    assert(merged["AAPL"].totalVolume == 1200);
    assert(merged["AAPL"].minPrice == 10.0);
    assert(merged["AAPL"].maxPrice == 20.0);
    assert(merged["AAPL"].averagePrice == 15.0);
}
