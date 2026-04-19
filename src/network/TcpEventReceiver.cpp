#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "network/TcpEventReceiver.h"

#include "api/protocol/PortProtocol.h"
#include "util/Clock.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

namespace mdp::network
{
    namespace
    {
        constexpr SOCKET invalidSocket()
        {
            return INVALID_SOCKET;
        }

        class WinsockSession
        {
        public:
            WinsockSession()
            {
                WSADATA data{};
                const int result = WSAStartup(MAKEWORD(2, 2), &data);
                if (result != 0)
                {
                    throw std::runtime_error("WSAStartup failed");
                }
            }

            ~WinsockSession()
            {
                WSACleanup();
            }
        };

        bool receiveExact(SOCKET socket, void* buffer, int byteCount)
        {
            char* out = static_cast<char*>(buffer);
            int received = 0;

            while (received < byteCount)
            {
                const int result = recv(socket, out + received, byteCount - received, 0);
                if (result <= 0)
                {
                    return false;
                }

                received += result;
            }

            return true;
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

        void closeSocket(std::atomic<std::uintptr_t>& socketStorage)
        {
            const auto rawSocket = socketStorage.exchange(0);
            if (rawSocket != 0)
            {
                closesocket(static_cast<SOCKET>(rawSocket));
            }
        }

        protocol::RejectPayload makeReject(
            std::uint64_t messageSequence,
            protocol::RejectReason reason)
        {
            protocol::RejectPayload reject;
            reject.rejectedMessageSequence = messageSequence;
            reject.reason = reason;
            return reject;
        }
    }

    TcpEventReceiver::TcpEventReceiver(
        pipeline::IEventRouter& eventRouter,
        TcpEventReceiverOptions options)
        : m_eventRouter(eventRouter)
        , m_options(std::move(options))
    {
    }

    TcpEventReceiver::~TcpEventReceiver()
    {
        stop();
    }

    void TcpEventReceiver::start()
    {
        if (m_running.exchange(true))
        {
            return;
        }

        m_thread = std::thread(&TcpEventReceiver::receiveLoop, this);
    }

    void TcpEventReceiver::requestStop()
    {
        m_running.store(false);
        closeSocket(m_listenSocket);
    }

    void TcpEventReceiver::stop()
    {
        requestStop();
        join();
    }

    void TcpEventReceiver::join()
    {
        if (m_thread.joinable())
        {
            m_thread.join();
        }

        joinClientThreads();
    }

    std::uint64_t TcpEventReceiver::getReceivedEventCount() const
    {
        return m_receivedEventCount.load();
    }

    std::uint64_t TcpEventReceiver::getSubmittedEventCount() const
    {
        return m_submittedEventCount.load();
    }

    std::uint64_t TcpEventReceiver::getRejectedEventCount() const
    {
        return m_rejectedEventCount.load();
    }

    std::uint64_t TcpEventReceiver::getDecodeFailureCount() const
    {
        return m_decodeFailureCount.load();
    }

    std::uint64_t TcpEventReceiver::getAcceptedConnectionCount() const
    {
        return m_acceptedConnectionCount.load();
    }

    std::uint64_t TcpEventReceiver::getRejectedMessageCount() const
    {
        return m_rejectedMessageCount.load();
    }

    void TcpEventReceiver::joinClientThreads()
    {
        std::vector<std::thread> clientThreads;
        {
            std::lock_guard<std::mutex> lock(m_clientThreadsMutex);
            clientThreads.swap(m_clientThreads);
        }

        for (auto& clientThread : clientThreads)
        {
            if (clientThread.joinable())
            {
                clientThread.join();
            }
        }
    }

    void TcpEventReceiver::receiveLoop()
    {
        try
        {
            WinsockSession winsock;

            SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (listenSocket == invalidSocket())
            {
                throw std::runtime_error("Failed to create listen socket");
            }

            m_listenSocket.store(static_cast<std::uintptr_t>(listenSocket));

            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_port = htons(m_options.listenPort);

            if (inet_pton(AF_INET, m_options.listenAddress.c_str(), &address.sin_addr) != 1)
            {
                throw std::runtime_error("Invalid listen address: " + m_options.listenAddress);
            }

            int reuse = 1;
            setsockopt(
                listenSocket,
                SOL_SOCKET,
                SO_REUSEADDR,
                reinterpret_cast<const char*>(&reuse),
                sizeof(reuse));

            if (bind(listenSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) ==
                SOCKET_ERROR)
            {
                throw std::runtime_error("Failed to bind listen socket");
            }

            if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR)
            {
                throw std::runtime_error("Failed to listen on socket");
            }

            while (m_running.load())
            {
                SOCKET clientSocket = accept(listenSocket, nullptr, nullptr);
                if (clientSocket == invalidSocket())
                {
                    if (!m_running.load())
                    {
                        break;
                    }

                    continue;
                }

                if (m_activeConnectionCount.load() >= m_options.maxConnections)
                {
                    m_rejectedMessageCount.fetch_add(1);
                    closesocket(clientSocket);
                    continue;
                }

                m_acceptedConnectionCount.fetch_add(1);
                m_activeConnectionCount.fetch_add(1);

                {
                    std::lock_guard<std::mutex> lock(m_clientThreadsMutex);
                    m_clientThreads.emplace_back(
                        &TcpEventReceiver::handleClient,
                        this,
                        static_cast<std::uintptr_t>(clientSocket));
                }
            }
        }
        catch (const std::exception& ex)
        {
            if (m_running.load())
            {
                std::cerr << "TCP receiver stopped: " << ex.what() << '\n';
            }
        }

        closeSocket(m_listenSocket);
        joinClientThreads();
    }

    void TcpEventReceiver::handleClient(std::uintptr_t rawClientSocket)
    {
        const SOCKET clientSocket = static_cast<SOCKET>(rawClientSocket);

        try
        {
            protocol::MessageHeader helloHeader;
            protocol::ClientHelloPayload helloPayload;
            if (!receiveExact(clientSocket, &helloHeader, sizeof(helloHeader)) ||
                !protocol::isValidMessageHeader(helloHeader) ||
                helloHeader.type != protocol::MessageType::ClientHello ||
                helloHeader.payloadSize != sizeof(helloPayload) ||
                !receiveExact(clientSocket, &helloPayload, sizeof(helloPayload)))
            {
                m_rejectedMessageCount.fetch_add(1);
                closesocket(clientSocket);
                m_activeConnectionCount.fetch_sub(1);
                return;
            }

            protocol::ServerHelloPayload serverHello;
            serverHello.maxEventFramesPerBatch = static_cast<std::uint32_t>(
                std::min<std::size_t>(
                    m_options.maxBatchSize,
                    protocol::kDefaultMaxEventFramesPerBatch));
            protocol::MessageHeader serverHelloHeader = protocol::makeMessageHeader(
                protocol::MessageType::ServerHello,
                sizeof(serverHello),
                1);

            if (!sendExact(clientSocket, &serverHelloHeader, sizeof(serverHelloHeader)) ||
                !sendExact(clientSocket, &serverHello, sizeof(serverHello)))
            {
                closesocket(clientSocket);
                m_activeConnectionCount.fetch_sub(1);
                return;
            }

            while (m_running.load())
            {
                protocol::MessageHeader header;
                if (!receiveExact(clientSocket, &header, sizeof(header)))
                {
                    break;
                }

                if (!protocol::isValidMessageHeader(header))
                {
                    m_rejectedMessageCount.fetch_add(1);
                    const auto reject = makeReject(
                        header.messageSequence,
                        protocol::RejectReason::UnsupportedVersion);
                    const auto rejectHeader = protocol::makeMessageHeader(
                        protocol::MessageType::Reject,
                        sizeof(reject),
                        header.messageSequence);
                    sendExact(clientSocket, &rejectHeader, sizeof(rejectHeader));
                    sendExact(clientSocket, &reject, sizeof(reject));
                    break;
                }

                if (header.type == protocol::MessageType::Heartbeat)
                {
                    std::vector<char> payload(header.payloadSize);
                    if (header.payloadSize > 0 &&
                        !receiveExact(
                            clientSocket,
                            payload.data(),
                            static_cast<int>(payload.size())))
                    {
                        break;
                    }

                    continue;
                }

                if (header.type != protocol::MessageType::EventBatch)
                {
                    m_rejectedMessageCount.fetch_add(1);
                    break;
                }

                protocol::EventBatchPayloadHeader batchHeader;
                if (header.payloadSize < sizeof(batchHeader) ||
                    !receiveExact(clientSocket, &batchHeader, sizeof(batchHeader)))
                {
                    m_rejectedMessageCount.fetch_add(1);
                    break;
                }

                const std::uint32_t expectedPayloadSize =
                    protocol::eventBatchPayloadSize(batchHeader.eventFrameCount);

                if (batchHeader.eventFrameCount == 0 ||
                    batchHeader.eventFrameCount > m_options.maxBatchSize ||
                    batchHeader.eventFrameSize != sizeof(protocol::MarketDataEventFrame) ||
                    header.payloadSize != expectedPayloadSize)
                {
                    m_rejectedMessageCount.fetch_add(1);
                    break;
                }

                std::vector<protocol::MarketDataEventFrame> frames(
                    batchHeader.eventFrameCount);
                if (!receiveExact(
                    clientSocket,
                    frames.data(),
                    static_cast<int>(
                        frames.size() * sizeof(protocol::MarketDataEventFrame))))
                {
                    m_rejectedMessageCount.fetch_add(1);
                    break;
                }

                const TimestampNs batchIngestTimestampNs = clock::nowNs();
                std::uint64_t receivedInBatch = 0;
                std::uint64_t acceptedInBatch = 0;
                std::uint64_t rejectedInBatch = 0;
                std::uint64_t decodeFailuresInBatch = 0;

                for (const auto& frame : frames)
                {
                    MarketDataEvent event;
                    const auto decodeResult =
                        protocol::decodeMarketDataEventFrame(frame, event);

                    if (!decodeResult.ok)
                    {
                        ++decodeFailuresInBatch;
                        continue;
                    }

                    event.ingestTimestampNs = batchIngestTimestampNs;
                    ++receivedInBatch;

                    if (m_eventRouter.submit(event))
                    {
                        ++acceptedInBatch;
                    }
                    else
                    {
                        ++rejectedInBatch;
                    }
                }

                m_receivedEventCount.fetch_add(receivedInBatch);
                m_submittedEventCount.fetch_add(acceptedInBatch);
                m_rejectedEventCount.fetch_add(rejectedInBatch);
                m_decodeFailureCount.fetch_add(decodeFailuresInBatch);

                protocol::AckPayload ack;
                ack.acknowledgedMessageSequence = header.messageSequence;
                ack.acceptedEventCount = acceptedInBatch;
                const auto ackHeader = protocol::makeMessageHeader(
                    protocol::MessageType::Ack,
                    sizeof(ack),
                    header.messageSequence);

                if (!sendExact(clientSocket, &ackHeader, sizeof(ackHeader)) ||
                    !sendExact(clientSocket, &ack, sizeof(ack)))
                {
                    break;
                }
            }
        }
        catch (const std::exception& ex)
        {
            if (m_running.load())
            {
                std::cerr << "TCP client session stopped: " << ex.what() << '\n';
            }
        }

        closesocket(clientSocket);
        m_activeConnectionCount.fetch_sub(1);
    }
}
