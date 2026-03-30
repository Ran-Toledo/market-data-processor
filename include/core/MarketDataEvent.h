// MarketDataEvent.h
#pragma once

#include "core/Types.h"

namespace mdp
{
    struct MarketDataEvent
    {
        Symbol symbol{};
        double price{ 0.0 };
        std::uint32_t volume{ 0 };
        TimestampNs exchangeTimestampNs{ 0 };
        TimestampNs ingestTimestampNs{ 0 };
        SequenceNumber sequenceNumber{ 0 };
    };
}
