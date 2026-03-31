// MarketDataEvent.h
#pragma once

#include "core/Types.h"

namespace mdp
{
    struct MarketDataEvent
    {
        Symbol symbol;
        double price;
        uint32_t volume;
        TimestampNs exchangeTimestampNs;
        TimestampNs ingestTimestampNs;
        SequenceNumber sequenceNumber;
    };
}
