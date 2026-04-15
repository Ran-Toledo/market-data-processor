#pragma once

#include "api/protocol/MarketDataEventFrame.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace mdp::protocol
{
    inline constexpr std::array<char, 4> kPortProtocolMagic{ 'M', 'D', 'P', 'N' };
    inline constexpr std::uint16_t kPortProtocolVersion = 1;
    inline constexpr std::string_view kDefaultListenAddress = "127.0.0.1";
    inline constexpr std::uint16_t kDefaultListenPort = 19000;
    inline constexpr std::uint32_t kDefaultHeartbeatIntervalMs = 1000;
    inline constexpr std::uint32_t kDefaultMaxEventFramesPerBatch = 256;

    enum class MessageType : std::uint16_t
    {
        ClientHello = 1,
        ServerHello = 2,
        EventBatch = 3,
        Heartbeat = 4,
        Ack = 5,
        Reject = 6
    };

    enum class EndpointRole : std::uint16_t
    {
        Publisher = 1,
        Processor = 2
    };

    enum class RejectReason : std::uint16_t
    {
        None = 0,
        InvalidMagic = 1,
        UnsupportedVersion = 2,
        InvalidMessageType = 3,
        InvalidPayloadSize = 4,
        UnsupportedSource = 5,
        InternalError = 6
    };

    struct MessageHeader
    {
        std::array<char, 4> magic{ kPortProtocolMagic };
        std::uint16_t version{ kPortProtocolVersion };
        std::uint16_t headerSize{ static_cast<std::uint16_t>(sizeof(MessageHeader)) };
        MessageType type{ MessageType::Heartbeat };
        std::uint16_t flags{ 0 };
        std::uint32_t payloadSize{ 0 };
        std::uint64_t messageSequence{ 0 };
    };

    static_assert(sizeof(MessageHeader) == 24);

    struct ClientHelloPayload
    {
        EndpointRole role{ EndpointRole::Publisher };
        std::uint16_t reserved{ 0 };
        std::uint32_t requestedHeartbeatIntervalMs{ kDefaultHeartbeatIntervalMs };
        std::uint32_t maxEventFramesPerBatch{ kDefaultMaxEventFramesPerBatch };
        std::uint32_t reserved2{ 0 };
    };

    static_assert(sizeof(ClientHelloPayload) == 16);

    struct ServerHelloPayload
    {
        EndpointRole role{ EndpointRole::Processor };
        std::uint16_t reserved{ 0 };
        std::uint32_t acceptedHeartbeatIntervalMs{ kDefaultHeartbeatIntervalMs };
        std::uint32_t maxEventFramesPerBatch{ kDefaultMaxEventFramesPerBatch };
        std::uint32_t reserved2{ 0 };
    };

    static_assert(sizeof(ServerHelloPayload) == 16);

    struct EventBatchPayloadHeader
    {
        std::uint32_t eventFrameCount{ 0 };
        std::uint32_t eventFrameSize{ kMarketDataEventFrameSize };
        std::uint64_t firstEventSequenceNumber{ 0 };
    };

    static_assert(sizeof(EventBatchPayloadHeader) == 16);

    struct HeartbeatPayload
    {
        std::uint64_t timestampNs{ 0 };
    };

    static_assert(sizeof(HeartbeatPayload) == 8);

    struct AckPayload
    {
        std::uint64_t acknowledgedMessageSequence{ 0 };
        std::uint64_t acceptedEventCount{ 0 };
    };

    static_assert(sizeof(AckPayload) == 16);

    struct RejectPayload
    {
        std::uint64_t rejectedMessageSequence{ 0 };
        RejectReason reason{ RejectReason::None };
        std::uint16_t reserved{ 0 };
        std::uint32_t detailCode{ 0 };
    };

    static_assert(sizeof(RejectPayload) == 16);

    inline constexpr std::uint32_t eventBatchPayloadSize(std::uint32_t eventFrameCount)
    {
        return static_cast<std::uint32_t>(
            sizeof(EventBatchPayloadHeader) +
            (static_cast<std::size_t>(eventFrameCount) * sizeof(MarketDataEventFrame)));
    }

    inline MessageHeader makeMessageHeader(
        MessageType type,
        std::uint32_t payloadSize,
        std::uint64_t messageSequence)
    {
        MessageHeader header;
        header.type = type;
        header.payloadSize = payloadSize;
        header.messageSequence = messageSequence;
        return header;
    }

    inline bool isValidMessageHeader(const MessageHeader& header)
    {
        return header.magic == kPortProtocolMagic &&
            header.version == kPortProtocolVersion &&
            header.headerSize == sizeof(MessageHeader);
    }
}
