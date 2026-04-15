#include "api/protocol/MarketDataEventFrame.h"

#include <cassert>
#include <limits>

namespace
{
    mdp::MarketDataEvent makeEvent(const mdp::Symbol& symbol = "AAPL")
    {
        mdp::MarketDataEvent event;
        event.symbol = symbol;
        event.price = 185.25;
        event.volume = 100;
        event.exchangeTimestampNs = 123456789;
        event.ingestTimestampNs = 987654321;
        event.enqueueTimestampNs = 555;
        event.sequenceNumber = 42;
        return event;
    }

    void testFrameRoundTrip()
    {
        mdp::protocol::MarketDataEventFrame frame;
        const mdp::MarketDataEvent sourceEvent = makeEvent();

        assert(mdp::protocol::encodeMarketDataEventFrame(sourceEvent, frame));
        assert(frame.magic == mdp::protocol::kMarketDataEventFrameMagic);
        assert(frame.version == mdp::protocol::kMarketDataEventFrameVersion);
        assert(frame.frameSize == mdp::protocol::kMarketDataEventFrameSize);

        mdp::MarketDataEvent decodedEvent;
        const auto result =
            mdp::protocol::decodeMarketDataEventFrame(frame, decodedEvent);

        assert(result.ok);
        assert(result.error == mdp::protocol::DecodeError::None);
        assert(decodedEvent.symbol == sourceEvent.symbol);
        assert(decodedEvent.price == sourceEvent.price);
        assert(decodedEvent.volume == sourceEvent.volume);
        assert(decodedEvent.exchangeTimestampNs == sourceEvent.exchangeTimestampNs);
        assert(decodedEvent.sequenceNumber == sourceEvent.sequenceNumber);
        assert(decodedEvent.ingestTimestampNs == 0);
        assert(decodedEvent.enqueueTimestampNs == 0);
    }

    void testFullWidthSymbolRoundTrip()
    {
        mdp::protocol::MarketDataEventFrame frame;
        const mdp::MarketDataEvent sourceEvent = makeEvent("SYMBOL1234567890");

        assert(sourceEvent.symbol.size() == mdp::protocol::kMarketDataEventFrameSymbolSize);
        assert(mdp::protocol::encodeMarketDataEventFrame(sourceEvent, frame));

        mdp::MarketDataEvent decodedEvent;
        const auto result =
            mdp::protocol::decodeMarketDataEventFrame(frame, decodedEvent);

        assert(result.ok);
        assert(decodedEvent.symbol == sourceEvent.symbol);
    }

    void testRejectsInvalidHeaderFields()
    {
        mdp::protocol::MarketDataEventFrame frame;
        assert(mdp::protocol::encodeMarketDataEventFrame(makeEvent(), frame));

        mdp::MarketDataEvent decodedEvent;

        auto invalidMagicFrame = frame;
        invalidMagicFrame.magic = { 'B', 'A', 'D', '!' };
        auto result =
            mdp::protocol::decodeMarketDataEventFrame(invalidMagicFrame, decodedEvent);
        assert(!result.ok);
        assert(result.error == mdp::protocol::DecodeError::InvalidMagic);

        auto invalidVersionFrame = frame;
        invalidVersionFrame.version = 2;
        result =
            mdp::protocol::decodeMarketDataEventFrame(invalidVersionFrame, decodedEvent);
        assert(!result.ok);
        assert(result.error == mdp::protocol::DecodeError::UnsupportedVersion);

        auto invalidSizeFrame = frame;
        invalidSizeFrame.frameSize = 0;
        result =
            mdp::protocol::decodeMarketDataEventFrame(invalidSizeFrame, decodedEvent);
        assert(!result.ok);
        assert(result.error == mdp::protocol::DecodeError::InvalidFrameSize);
    }

    void testRejectsInvalidPayloadFields()
    {
        mdp::protocol::MarketDataEventFrame frame;

        assert(!mdp::protocol::encodeMarketDataEventFrame(makeEvent(""), frame));
        assert(!mdp::protocol::encodeMarketDataEventFrame(
            makeEvent("SYMBOL12345678901"),
            frame));

        auto invalidPriceEvent = makeEvent();
        invalidPriceEvent.price = std::numeric_limits<double>::quiet_NaN();
        assert(!mdp::protocol::encodeMarketDataEventFrame(invalidPriceEvent, frame));

        assert(mdp::protocol::encodeMarketDataEventFrame(makeEvent(), frame));
        frame.volume = 0;

        mdp::MarketDataEvent decodedEvent;
        const auto result =
            mdp::protocol::decodeMarketDataEventFrame(frame, decodedEvent);

        assert(!result.ok);
        assert(result.error == mdp::protocol::DecodeError::InvalidVolume);
    }
}

void runMarketDataEventFrameTests()
{
    testFrameRoundTrip();
    testFullWidthSymbolRoundTrip();
    testRejectsInvalidHeaderFields();
    testRejectsInvalidPayloadFields();
}
