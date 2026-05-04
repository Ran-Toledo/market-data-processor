#pragma once

#include "api/domain/MarketDataEvent.h"
#include "ibkr/IbkrQuote.h"

#include <optional>
#include <string>

namespace mdp::publisher
{
    class IbkrQuoteMapper
    {
    public:
        struct MapResult
        {
            bool usedMidpoint{ false };
            bool usedQuoteSize{ false };
            MarketDataEvent event;
        };

        static std::optional<MapResult> mapQuote(
            const IbkrQuote& quote,
            const std::string& symbol,
            SequenceNumber sequenceNumber);
    };
}
