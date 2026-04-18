#pragma once

#include "pipeline/IEventRouter.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <thread>

namespace mdp::network
{
    struct TcpEventReceiverOptions
    {
        std::string listenAddress{ "127.0.0.1" };
        std::uint16_t listenPort{ 19000 };
        std::size_t maxBatchSize{ 256 };
    };

    class TcpEventReceiver
    {
    public:
        TcpEventReceiver(
            pipeline::IEventRouter& eventRouter,
            TcpEventReceiverOptions options);
        ~TcpEventReceiver();

        void start();
        void requestStop();
        void stop();
        void join();

        std::uint64_t getReceivedEventCount() const;
        std::uint64_t getSubmittedEventCount() const;
        std::uint64_t getRejectedEventCount() const;
        std::uint64_t getDecodeFailureCount() const;
        std::uint64_t getAcceptedConnectionCount() const;
        std::uint64_t getRejectedMessageCount() const;

    private:
        void receiveLoop();

        pipeline::IEventRouter& m_eventRouter;
        TcpEventReceiverOptions m_options;
        std::thread m_thread;
        std::atomic<bool> m_running{ false };
        std::atomic<std::uint64_t> m_receivedEventCount{ 0 };
        std::atomic<std::uint64_t> m_submittedEventCount{ 0 };
        std::atomic<std::uint64_t> m_rejectedEventCount{ 0 };
        std::atomic<std::uint64_t> m_decodeFailureCount{ 0 };
        std::atomic<std::uint64_t> m_acceptedConnectionCount{ 0 };
        std::atomic<std::uint64_t> m_rejectedMessageCount{ 0 };
        std::atomic<std::uintptr_t> m_listenSocket{ 0 };
        std::atomic<std::uintptr_t> m_clientSocket{ 0 };
    };
}
