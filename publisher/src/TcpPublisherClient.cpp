#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "TcpPublisherClient.h"

#include "api/protocol/PortProtocol.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <stdexcept>
#include <thread>
#include <utility>

namespace mdp::publisher
{
    namespace
    {
        class WinsockSession
        {
        public:
            WinsockSession()
            {
                WSADATA data{};
                if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
                {
                    throw std::runtime_error("WSAStartup failed");
                }
            }

            ~WinsockSession()
            {
                WSACleanup();
            }
        };

        WinsockSession& winsock()
        {
            static WinsockSession session;
            return session;
        }

        bool sendExact(SOCKET socket, const void* buffer, int byteCount)
        {
            const char* input = static_cast<const char*>(buffer);
            int sent = 0;

            while (sent < byteCount)
            {
                const int result = send(socket, input + sent, byteCount - sent, 0);
                if (result <= 0)
                {
                    return false;
                }

                sent += result;
            }

            return true;
        }

        bool receiveExact(SOCKET socket, void* buffer, int byteCount)
        {
            char* output = static_cast<char*>(buffer);
            int received = 0;

            while (received < byteCount)
            {
                const int result = recv(socket, output + received, byteCount - received, 0);
                if (result <= 0)
                {
                    return false;
                }

                received += result;
            }

            return true;
        }
    }

    TcpPublisherClient::TcpPublisherClient(TcpPublisherClientOptions options)
        : m_options(std::move(options))
    {
        (void)winsock();
    }

    TcpPublisherClient::~TcpPublisherClient()
    {
        close();
    }

    void TcpPublisherClient::connect()
    {
        close();
        m_inFlightMessageSequences.clear();

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(m_options.processorPort);

        if (inet_pton(AF_INET, m_options.processorHost.c_str(), &address.sin_addr) != 1)
        {
            throw std::runtime_error("Invalid processor host: " + m_options.processorHost);
        }

        SOCKET socketHandle = INVALID_SOCKET;

        while (socketHandle == INVALID_SOCKET)
        {
            socketHandle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (socketHandle == INVALID_SOCKET)
            {
                throw std::runtime_error("Failed to create publisher socket");
            }

            if (::connect(
                socketHandle,
                reinterpret_cast<sockaddr*>(&address),
                sizeof(address)) == 0)
            {
                break;
            }

            closesocket(socketHandle);
            socketHandle = INVALID_SOCKET;
            std::this_thread::sleep_for(
                std::chrono::milliseconds(m_options.connectRetryMs));
        }

        m_socket = static_cast<std::uintptr_t>(socketHandle);

        protocol::ClientHelloPayload hello;
        hello.maxEventFramesPerBatch = m_options.maxBatchSize;
        const auto helloHeader = protocol::makeMessageHeader(
            protocol::MessageType::ClientHello,
            sizeof(hello),
            m_nextMessageSequence++);

        if (!sendExact(socketHandle, &helloHeader, sizeof(helloHeader)) ||
            !sendExact(socketHandle, &hello, sizeof(hello)))
        {
            throw std::runtime_error("Failed to send client hello");
        }

        protocol::MessageHeader serverHeader;
        protocol::ServerHelloPayload serverHello;
        if (!receiveExact(socketHandle, &serverHeader, sizeof(serverHeader)) ||
            !protocol::isValidMessageHeader(serverHeader) ||
            serverHeader.type != protocol::MessageType::ServerHello ||
            serverHeader.payloadSize != sizeof(serverHello) ||
            !receiveExact(socketHandle, &serverHello, sizeof(serverHello)))
        {
            throw std::runtime_error("Failed to receive server hello");
        }

        m_options.maxBatchSize = std::max<std::uint32_t>(
            1,
            std::min(m_options.maxBatchSize, serverHello.maxEventFramesPerBatch));
        m_options.ackWindowBatches = std::max<std::size_t>(1, m_options.ackWindowBatches);
    }

    void TcpPublisherClient::close()
    {
        if (m_socket != 0)
        {
            closesocket(static_cast<SOCKET>(m_socket));
            m_socket = 0;
        }
    }

    std::uint32_t TcpPublisherClient::maxBatchSize() const
    {
        return m_options.maxBatchSize;
    }

    std::uint64_t TcpPublisherClient::sendBatch(
        const std::vector<protocol::MarketDataEventFrame>& frames)
    {
        if (m_socket == 0)
        {
            throw std::runtime_error("Publisher socket is not connected");
        }

        if (frames.empty() || frames.size() > m_options.maxBatchSize)
        {
            throw std::runtime_error("Invalid event batch size");
        }

        protocol::EventBatchPayloadHeader batchHeader;
        batchHeader.eventFrameCount = static_cast<std::uint32_t>(frames.size());
        batchHeader.eventFrameSize = sizeof(protocol::MarketDataEventFrame);
        batchHeader.firstEventSequenceNumber = frames.front().sequenceNumber;

        const std::uint64_t messageSequence = m_nextMessageSequence++;
        const auto header = protocol::makeMessageHeader(
            protocol::MessageType::EventBatch,
            protocol::eventBatchPayloadSize(batchHeader.eventFrameCount),
            messageSequence);

        const SOCKET socketHandle = static_cast<SOCKET>(m_socket);
        if (!sendExact(socketHandle, &header, sizeof(header)) ||
            !sendExact(socketHandle, &batchHeader, sizeof(batchHeader)) ||
            !sendExact(
                socketHandle,
                frames.data(),
                static_cast<int>(frames.size() * sizeof(frames.front()))))
        {
            throw std::runtime_error("Failed to send event batch");
        }

        m_inFlightMessageSequences.push_back(messageSequence);
        if (m_inFlightMessageSequences.size() < m_options.ackWindowBatches)
        {
            return 0;
        }

        return receiveNextAck();
    }

    std::uint64_t TcpPublisherClient::flushAcks()
    {
        std::uint64_t acceptedCount = 0;
        while (!m_inFlightMessageSequences.empty())
        {
            acceptedCount += receiveNextAck();
        }

        return acceptedCount;
    }

    std::uint64_t TcpPublisherClient::receiveNextAck()
    {
        if (m_socket == 0)
        {
            throw std::runtime_error("Publisher socket is not connected");
        }

        if (m_inFlightMessageSequences.empty())
        {
            return 0;
        }

        const std::uint64_t expectedMessageSequence = m_inFlightMessageSequences.front();
        protocol::MessageHeader ackHeader;
        protocol::AckPayload ack;
        const SOCKET socketHandle = static_cast<SOCKET>(m_socket);
        if (!receiveExact(socketHandle, &ackHeader, sizeof(ackHeader)) ||
            !protocol::isValidMessageHeader(ackHeader) ||
            ackHeader.type != protocol::MessageType::Ack ||
            ackHeader.payloadSize != sizeof(ack) ||
            !receiveExact(socketHandle, &ack, sizeof(ack)) ||
            ack.acknowledgedMessageSequence != expectedMessageSequence)
        {
            throw std::runtime_error("Failed to receive event batch ack");
        }

        m_inFlightMessageSequences.pop_front();
        return ack.acceptedEventCount;
    }
}
