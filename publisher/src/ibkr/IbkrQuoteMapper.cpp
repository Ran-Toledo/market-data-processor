#include "ibkr/IbkrQuoteMapper.h"

#include <algorithm>
#include <chrono>

namespace mdp::publisher
{
    namespace
    {
        TimestampNs nowNs()
        {
            const auto now = std::chrono::steady_clock::now().time_since_epoch();
            return static_cast<TimestampNs>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
        }
    }

    std::optional<IbkrQuoteMapper::MapResult> IbkrQuoteMapper::mapQuote(
        const IbkrQuote& quote,
        const std::string& symbol,
        SequenceNumber sequenceNumber)
    {
        if (symbol.empty() || sequenceNumber == 0)
        {
            return std::nullopt;
        }

        double price = 0.0;
        bool usedMidpoint = false;
        if (quote.lastPrice.has_value() && *quote.lastPrice > 0.0)
        {
            price = *quote.lastPrice;
        }
        else if (quote.bidPrice.has_value() &&
            quote.askPrice.has_value() &&
            *quote.bidPrice > 0.0 &&
            *quote.askPrice > 0.0)
        {
            price = (*quote.bidPrice + *quote.askPrice) / 2.0;
            usedMidpoint = true;
        }

        if (price <= 0.0)
        {
            return std::nullopt;
        }

        std::uint32_t volume = 1;
        bool usedQuoteSize = false;
        if (quote.bidSize.has_value() || quote.askSize.has_value())
        {
            const std::uint32_t bidSize = quote.bidSize.value_or(0);
            const std::uint32_t askSize = quote.askSize.value_or(0);
            volume = std::max<std::uint32_t>(1, std::max(bidSize, askSize));
            usedQuoteSize = true;
        }

        MarketDataEvent event;
        event.symbol = symbol;
        event.price = price;
        event.volume = volume;
        event.exchangeTimestampNs = quote.updatedMs.has_value()
            ? static_cast<TimestampNs>(*quote.updatedMs) * 1000000ULL
            : nowNs();
        event.ingestTimestampNs = nowNs();
        event.sequenceNumber = sequenceNumber;

        return MapResult{ usedMidpoint, usedQuoteSize, event };
    }
}
