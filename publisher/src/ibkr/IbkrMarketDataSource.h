#pragma once

#include "IPublisherSource.h"
#include "PublisherConfig.h"
#include "ibkr/IbkrMarketDataClient.h"
#include "ibkr/IbkrQuote.h"
#include "ibkr/IbkrWebSocketClient.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace mdp::publisher
{
    class IbkrMarketDataSource final : public IPublisherSource
    {
    public:
        explicit IbkrMarketDataSource(
            config::IbkrConfig config,
            std::unique_ptr<IIbkrMarketDataClient> client = nullptr,
            std::unique_ptr<IIbkrWebSocketClient> websocketClient = nullptr);
        ~IbkrMarketDataSource() override;

        bool next(MarketDataEvent& outEvent) override;
        std::chrono::milliseconds idleWaitHint() const override;

    private:
        enum class Transport
        {
            Snapshot,
            Websocket
        };

        void initialize();
        void resolveConfiguredContracts();
        void initializeSnapshot();
        void initializeWebsocket();
        void pollSnapshot();
        void startWebsocketThreads();
        void stopWebsocketThreads();
        void websocketReceiveLoop();
        void websocketMaintenanceLoop();
        void enqueueQuoteEvent(const IbkrQuote& quote);
        std::string resolveSymbol(const std::string& conid) const;
        std::string websocketUrl() const;
        Transport transport() const;

    private:
        config::IbkrConfig m_config;
        std::unique_ptr<IIbkrMarketDataClient> m_client;
        std::unique_ptr<IIbkrWebSocketClient> m_websocketClient;
        mutable std::mutex m_bufferMutex;
        std::deque<MarketDataEvent> m_buffer;
        std::unordered_map<std::string, SequenceNumber> m_sequenceBySymbol;
        std::unordered_map<std::string, IbkrQuote> m_cachedQuotesByConid;
        std::chrono::steady_clock::time_point m_nextPollTime;
        std::atomic<bool> m_running{ false };
        std::thread m_receiveThread;
        std::thread m_maintenanceThread;
    };
}
