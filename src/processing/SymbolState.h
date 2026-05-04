#pragma once

#include "api/domain/Types.h"

#include <cstdint>

namespace mdp::processing
{
    struct SymbolState
    {
        double lastPrice = 0.0;
        std::uint32_t lastVolume = 0;
        TimestampNs lastExchangeTimestampNs = 0;
        TimestampNs lastIngestTimestampNs = 0;
        SequenceNumber lastSequenceNumber = 0;
    };
}
