#pragma once

#include "api/domain/Types.h"

#include <ostream>

namespace mdp
{
    struct MarketDataEvent
    {
        Symbol symbol;
        double price;
        std::uint32_t volume;
        TimestampNs exchangeTimestampNs;
        TimestampNs ingestTimestampNs;
        TimestampNs enqueueTimestampNs{ 0 };
        SequenceNumber sequenceNumber;
    };

    inline std::ostream& operator<<(std::ostream& os, const MarketDataEvent& event)
    {
        os << "Symbol: " << event.symbol
            << ", Price: " << event.price
            << ", Volume: " << event.volume
            << ", ExchangeTsNs: " << event.exchangeTimestampNs
            << ", IngestTsNs: " << event.ingestTimestampNs
            << ", EnqueueTsNs: " << event.enqueueTimestampNs
            << ", SeqNum: " << event.sequenceNumber;

        return os;
    }
}
