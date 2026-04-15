#include "api/protocol/MarketDataEventFrame.h"

#include <algorithm>
#include <cmath>

namespace mdp::protocol
{
    namespace
    {
        bool isValidPrice(double price)
        {
            return std::isfinite(price) && price > 0.0;
        }

        std::size_t getSymbolLength(
            const std::array<char, kMarketDataEventFrameSymbolSize>& symbol)
        {
            const auto terminator = std::find(symbol.begin(), symbol.end(), '\0');
            return static_cast<std::size_t>(terminator - symbol.begin());
        }
    }

    bool encodeMarketDataEventFrame(
        const MarketDataEvent& event,
        MarketDataEventFrame& outFrame)
    {
        if (event.symbol.empty() ||
            event.symbol.size() > kMarketDataEventFrameSymbolSize ||
            !isValidPrice(event.price) ||
            event.volume == 0 ||
            event.exchangeTimestampNs == 0 ||
            event.sequenceNumber == 0)
        {
            return false;
        }

        outFrame = {};
        outFrame.magic = kMarketDataEventFrameMagic;
        outFrame.version = kMarketDataEventFrameVersion;
        outFrame.frameSize = kMarketDataEventFrameSize;
        std::copy(event.symbol.begin(), event.symbol.end(), outFrame.symbol.begin());
        outFrame.price = event.price;
        outFrame.volume = event.volume;
        outFrame.flags = 0;
        outFrame.exchangeTimestampNs = event.exchangeTimestampNs;
        outFrame.sequenceNumber = event.sequenceNumber;

        return true;
    }

    DecodeResult decodeMarketDataEventFrame(
        const MarketDataEventFrame& frame,
        MarketDataEvent& outEvent)
    {
        if (frame.magic != kMarketDataEventFrameMagic)
        {
            return { false, DecodeError::InvalidMagic };
        }

        if (frame.version != kMarketDataEventFrameVersion)
        {
            return { false, DecodeError::UnsupportedVersion };
        }

        if (frame.frameSize != kMarketDataEventFrameSize)
        {
            return { false, DecodeError::InvalidFrameSize };
        }

        const std::size_t symbolLength = getSymbolLength(frame.symbol);
        if (symbolLength == 0)
        {
            return { false, DecodeError::EmptySymbol };
        }

        if (!isValidPrice(frame.price))
        {
            return { false, DecodeError::InvalidPrice };
        }

        if (frame.volume == 0)
        {
            return { false, DecodeError::InvalidVolume };
        }

        if (frame.exchangeTimestampNs == 0)
        {
            return { false, DecodeError::InvalidExchangeTimestamp };
        }

        if (frame.sequenceNumber == 0)
        {
            return { false, DecodeError::InvalidSequenceNumber };
        }

        outEvent = {};
        outEvent.symbol.assign(frame.symbol.data(), symbolLength);
        outEvent.price = frame.price;
        outEvent.volume = frame.volume;
        outEvent.exchangeTimestampNs = frame.exchangeTimestampNs;
        outEvent.sequenceNumber = frame.sequenceNumber;

        return { true, DecodeError::None };
    }
}
