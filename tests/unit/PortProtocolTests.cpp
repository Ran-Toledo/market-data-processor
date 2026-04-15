#include "api/protocol/PortProtocol.h"

#include <cassert>

namespace
{
    void testHeaderDefaults()
    {
        const auto header = mdp::protocol::makeMessageHeader(
            mdp::protocol::MessageType::ClientHello,
            sizeof(mdp::protocol::ClientHelloPayload),
            7);

        assert(header.magic == mdp::protocol::kPortProtocolMagic);
        assert(header.version == mdp::protocol::kPortProtocolVersion);
        assert(header.headerSize == sizeof(mdp::protocol::MessageHeader));
        assert(header.type == mdp::protocol::MessageType::ClientHello);
        assert(header.payloadSize == sizeof(mdp::protocol::ClientHelloPayload));
        assert(header.messageSequence == 7);
        assert(mdp::protocol::isValidMessageHeader(header));
    }

    void testHeaderValidationRejectsMismatches()
    {
        auto header = mdp::protocol::makeMessageHeader(
            mdp::protocol::MessageType::Heartbeat,
            sizeof(mdp::protocol::HeartbeatPayload),
            1);

        header.magic = { 'B', 'A', 'D', '!' };
        assert(!mdp::protocol::isValidMessageHeader(header));

        header = mdp::protocol::makeMessageHeader(
            mdp::protocol::MessageType::Heartbeat,
            sizeof(mdp::protocol::HeartbeatPayload),
            1);
        header.version = mdp::protocol::kPortProtocolVersion + 1;
        assert(!mdp::protocol::isValidMessageHeader(header));

        header = mdp::protocol::makeMessageHeader(
            mdp::protocol::MessageType::Heartbeat,
            sizeof(mdp::protocol::HeartbeatPayload),
            1);
        header.headerSize = 0;
        assert(!mdp::protocol::isValidMessageHeader(header));
    }

    void testEventBatchPayloadSizing()
    {
        const std::uint32_t oneFramePayload =
            mdp::protocol::eventBatchPayloadSize(1);
        assert(oneFramePayload ==
            sizeof(mdp::protocol::EventBatchPayloadHeader) +
            sizeof(mdp::protocol::MarketDataEventFrame));

        const std::uint32_t fourFramePayload =
            mdp::protocol::eventBatchPayloadSize(4);
        assert(fourFramePayload ==
            sizeof(mdp::protocol::EventBatchPayloadHeader) +
            (4 * sizeof(mdp::protocol::MarketDataEventFrame)));
    }

    void testDefaultEndpointSettings()
    {
        assert(mdp::protocol::kDefaultListenAddress == "127.0.0.1");
        assert(mdp::protocol::kDefaultListenPort == 19000);
        assert(mdp::protocol::kDefaultMaxEventFramesPerBatch == 256);
    }
}

void runPortProtocolTests()
{
    testHeaderDefaults();
    testHeaderValidationRejectsMismatches();
    testEventBatchPayloadSizing();
    testDefaultEndpointSettings();
}
