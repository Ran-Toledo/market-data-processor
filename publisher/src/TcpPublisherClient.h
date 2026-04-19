#pragma once

#include "api/protocol/MarketDataEventFrame.h"

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace mdp::publisher
{
    struct TcpPublisherClientOptions
    {
        std::string processorHost{ "127.0.0.1" };
        std::uint16_t processorPort{ 19000 };
        std::uint32_t connectRetryMs{ 1000 };
        std::uint32_t maxBatchSize{ 256 };
        std::size_t ackWindowBatches{ 1 };
    };

    class TcpPublisherClient
    {
    public:
        explicit TcpPublisherClient(TcpPublisherClientOptions options);
        ~TcpPublisherClient();

        void connect();
        void close();
        std::uint64_t sendBatch(const std::vector<protocol::MarketDataEventFrame>& frames);
        std::uint64_t flushAcks();
        std::uint32_t maxBatchSize() const;

    private:
        std::uint64_t receiveNextAck();

    private:
        TcpPublisherClientOptions m_options;
        std::uintptr_t m_socket{ 0 };
        std::uint64_t m_nextMessageSequence{ 1 };
        std::deque<std::uint64_t> m_inFlightMessageSequences;
    };
}
