#pragma once

#include "api/domain/MarketDataEvent.h"

#include <array>
#include <cstdint>

namespace mdp::protocol
{
    inline constexpr std::array<char, 4> kMarketDataEventFrameMagic{ 'M', 'D', 'P', '1' };
    inline constexpr std::uint16_t kMarketDataEventFrameVersion = 1;
    inline constexpr std::size_t kMarketDataEventFrameSymbolSize = 16;

    struct MarketDataEventFrame
    {
        std::array<char, 4> magic{};
        std::uint16_t version{ kMarketDataEventFrameVersion };
        std::uint16_t frameSize{ 0 };

        std::array<char, kMarketDataEventFrameSymbolSize> symbol{};
        double price{ 0.0 };
        std::uint32_t volume{ 0 };
        std::uint32_t flags{ 0 };

        TimestampNs exchangeTimestampNs{ 0 };
        SequenceNumber sequenceNumber{ 0 };
    };

    inline constexpr std::uint16_t kMarketDataEventFrameSize =
        static_cast<std::uint16_t>(sizeof(MarketDataEventFrame));

    static_assert(sizeof(MarketDataEventFrame) == 56);

    enum class DecodeError
    {
        None,
        InvalidMagic,
        UnsupportedVersion,
        InvalidFrameSize,
        EmptySymbol,
        InvalidPrice,
        InvalidVolume,
        InvalidExchangeTimestamp,
        InvalidSequenceNumber
    };

    struct DecodeResult
    {
        bool ok{ true };
        DecodeError error{ DecodeError::None };
    };

    bool encodeMarketDataEventFrame(
        const MarketDataEvent& event,
        MarketDataEventFrame& outFrame);

    DecodeResult decodeMarketDataEventFrame(
        const MarketDataEventFrame& frame,
        MarketDataEvent& outEvent);
}
