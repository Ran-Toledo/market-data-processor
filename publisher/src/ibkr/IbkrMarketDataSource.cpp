#include "ibkr/IbkrMarketDataSource.h"

#include "ibkr/IbkrQuoteMapper.h"
#include "ibkr/IbkrQuoteParser.h"

#include <algorithm>
#include <iostream>
#include <unordered_set>
#include <thread>

namespace mdp::publisher
{
    namespace
    {
        std::unique_ptr<IIbkrMarketDataClient> makeDefaultClient(
            const config::IbkrConfig& config)
        {
            return std::make_unique<IbkrMarketDataClient>(
                config,
                std::make_unique<WinHttpIbkrHttpClient>());
        }

        std::unique_ptr<IIbkrWebSocketClient> makeDefaultWebSocketClient()
        {
            return std::make_unique<WinHttpIbkrWebSocketClient>();
        }

        std::string trimTrailingSlash(std::string value)
        {
            while (!value.empty() && value.back() == '/')
            {
                value.pop_back();
            }

            return value;
        }
    }

    IbkrMarketDataSource::IbkrMarketDataSource(
        config::IbkrConfig config,
        std::unique_ptr<IIbkrMarketDataClient> client,
        std::unique_ptr<IIbkrWebSocketClient> websocketClient)
        : m_config(std::move(config))
        , m_client(std::move(client))
        , m_websocketClient(std::move(websocketClient))
        , m_nextPollTime(std::chrono::steady_clock::now())
    {
        if (m_client == nullptr)
        {
            m_client = makeDefaultClient(m_config);
        }

        if (m_websocketClient == nullptr)
        {
            m_websocketClient = makeDefaultWebSocketClient();
        }

        initialize();
    }

    IbkrMarketDataSource::~IbkrMarketDataSource()
    {
        stopWebsocketThreads();
    }

    bool IbkrMarketDataSource::next(MarketDataEvent& outEvent)
    {
        {
            std::lock_guard<std::mutex> lock(m_bufferMutex);
            if (!m_buffer.empty())
            {
                outEvent = m_buffer.front();
                m_buffer.pop_front();
                return true;
            }
        }

        if (transport() == Transport::Websocket)
        {
            return false;
        }

        const auto now = std::chrono::steady_clock::now();
        if (now < m_nextPollTime)
        {
            return false;
        }

        pollSnapshot();

        std::lock_guard<std::mutex> lock(m_bufferMutex);
        if (m_buffer.empty())
        {
            return false;
        }

        outEvent = m_buffer.front();
        m_buffer.pop_front();
        return true;
    }

    std::chrono::milliseconds IbkrMarketDataSource::idleWaitHint() const
    {
        {
            std::lock_guard<std::mutex> lock(m_bufferMutex);
            if (!m_buffer.empty())
            {
                return std::chrono::milliseconds(0);
            }
        }

        if (transport() == Transport::Websocket)
        {
            return std::chrono::milliseconds(5);
        }

        const auto now = std::chrono::steady_clock::now();
        if (now >= m_nextPollTime)
        {
            return std::chrono::milliseconds(0);
        }

        return std::chrono::duration_cast<std::chrono::milliseconds>(
            m_nextPollTime - now);
    }

    void IbkrMarketDataSource::initialize()
    {
        std::cout << "IBKR source enabled" << '\n';
        std::cout << "  transport="
            << (transport() == Transport::Snapshot ? "snapshot" : "websocket")
            << '\n';
        std::cout << "  base_url=" << m_config.baseUrl << '\n';
        std::cout << "  configured_symbols=" << m_config.symbols.size() << '\n';
        std::cout << "  configured_conids=" << m_config.conids.size() << '\n';
        std::cout << "  security_type=" << m_config.securityType << '\n';
        std::cout << "  poll_interval_ms=" << m_config.pollIntervalMs << '\n';
        std::cout << "  event_queue_capacity=" << m_config.eventQueueCapacity << '\n';
        std::cout << "  fields=";
        for (std::size_t i = 0; i < m_config.fields.size(); ++i)
        {
            if (i > 0)
            {
                std::cout << ',';
            }

            std::cout << m_config.fields[i];
        }
        std::cout << '\n';

        if (m_config.checkAuthOnStartup)
        {
            bool authenticated = false;
            std::string detail;
            const bool ok = m_client->checkAuthStatus(authenticated, detail);
            std::cout << "  auth_status=" << detail << '\n';
            if (!ok || !authenticated)
            {
                throw std::runtime_error(
                    "IBKR Client Portal API is not authenticated. Launch the "
                    "IBKR Gateway and authenticate externally before running "
                    "publisher with source=ibkr.");
            }
        }

        if (m_config.callAccountsOnStartup)
        {
            std::string detail;
            const bool ok = m_client->callAccounts(detail);
            std::cout << "  accounts_preflight=" << (ok ? "ok" : "failed")
                << " (" << detail << ')' << '\n';
        }

        resolveConfiguredContracts();

        if (transport() == Transport::Snapshot)
        {
            initializeSnapshot();
        }
        else
        {
            initializeWebsocket();
        }
    }

    void IbkrMarketDataSource::resolveConfiguredContracts()
    {
        std::vector<std::string> resolvedConids = m_config.conids;
        std::unordered_map<std::string, std::string> resolvedSymbolsByConid =
            m_config.symbolsByConid;
        std::unordered_set<std::string> seenConids(
            resolvedConids.begin(),
            resolvedConids.end());

        std::size_t resolvedSymbolCount = 0;
        std::size_t unresolvedSymbolCount = 0;

        for (const std::string& configuredSymbol : m_config.symbols)
        {
            try
            {
                const auto resolved = m_client->resolveContract(
                    configuredSymbol,
                    m_config.securityType);
                if (!resolved.has_value() || resolved->conid.empty())
                {
                    ++unresolvedSymbolCount;
                    std::cerr << "[ibkr] symbol resolution returned no conid for "
                        << configuredSymbol << '\n';
                    continue;
                }

                if (seenConids.insert(resolved->conid).second)
                {
                    resolvedConids.push_back(resolved->conid);
                }

                resolvedSymbolsByConid[resolved->conid] =
                    configuredSymbol.empty() ? resolved->symbol : configuredSymbol;
                ++resolvedSymbolCount;
            }
            catch (const std::exception& ex)
            {
                ++unresolvedSymbolCount;
                std::cerr << "[ibkr] symbol resolution failed for "
                    << configuredSymbol << ": " << ex.what() << '\n';
            }
        }

        m_config.conids = std::move(resolvedConids);
        m_config.symbolsByConid = std::move(resolvedSymbolsByConid);

        std::cout << "  resolved_conids=" << m_config.conids.size() << '\n';
        if (!m_config.symbols.empty())
        {
            std::cout << "  symbol_resolution resolved=" << resolvedSymbolCount
                << " unresolved=" << unresolvedSymbolCount << '\n';
        }

        if (m_config.conids.empty())
        {
            throw std::runtime_error(
                "IBKR source requires at least one configured symbol or conid "
                "that resolves successfully");
        }
    }

    void IbkrMarketDataSource::initializeSnapshot()
    {
        std::cout << "  snapshot polling configured" << '\n';
    }

    void IbkrMarketDataSource::initializeWebsocket()
    {
        bool authenticated = false;
        std::string sessionToken;
        std::string detail;
        if (!m_client->tickle(authenticated, sessionToken, detail) ||
            !authenticated ||
            sessionToken.empty())
        {
            throw std::runtime_error(
                "IBKR websocket preflight failed. The session may be expired or "
                "the Gateway may not be authenticated.");
        }

        m_websocketClient->connect(
            websocketUrl(),
            sessionToken,
            m_config.allowInsecureLocalhostTls);
        std::cout << "  websocket_url=" << websocketUrl() << '\n';

        for (const std::string& conid : m_config.conids)
        {
            std::string subscription = "smd+" + conid + "+{\"fields\":[";
            for (std::size_t i = 0; i < m_config.fields.size(); ++i)
            {
                if (i > 0)
                {
                    subscription += ',';
                }

                subscription += '"' + m_config.fields[i] + '"';
            }
            subscription += "]}";
            m_websocketClient->sendText(subscription);
        }

        startWebsocketThreads();
    }

    void IbkrMarketDataSource::pollSnapshot()
    {
        const auto now = std::chrono::steady_clock::now();
        m_nextPollTime = now + std::chrono::milliseconds(m_config.pollIntervalMs);

        try
        {
            const std::vector<IbkrQuote> quotes =
                m_client->fetchSnapshot(m_config.conids, m_config.fields);

            std::size_t producedCount = 0;
            std::size_t skippedCount = 0;
            for (const auto& quote : quotes)
            {
                const std::size_t beforeSize = [&]()
                {
                    std::lock_guard<std::mutex> lock(m_bufferMutex);
                    return m_buffer.size();
                }();

                enqueueQuoteEvent(quote);

                const std::size_t afterSize = [&]()
                {
                    std::lock_guard<std::mutex> lock(m_bufferMutex);
                    return m_buffer.size();
                }();

                if (afterSize > beforeSize)
                {
                    ++producedCount;
                }
                else
                {
                    ++skippedCount;
                }
            }

            std::cout << "[ibkr] snapshot quotes=" << quotes.size()
                << " produced_events=" << producedCount
                << " skipped_quotes=" << skippedCount << '\n';
        }
        catch (const std::exception& ex)
        {
            std::cerr << "[ibkr] snapshot request failed: "
                << ex.what() << '\n';
        }
    }

    void IbkrMarketDataSource::startWebsocketThreads()
    {
        m_running.store(true);
        m_receiveThread = std::thread(&IbkrMarketDataSource::websocketReceiveLoop, this);
        m_maintenanceThread =
            std::thread(&IbkrMarketDataSource::websocketMaintenanceLoop, this);
    }

    void IbkrMarketDataSource::stopWebsocketThreads()
    {
        if (transport() != Transport::Websocket)
        {
            return;
        }

        m_running.store(false);
        if (m_websocketClient != nullptr)
        {
            m_websocketClient->close();
        }

        if (m_receiveThread.joinable())
        {
            m_receiveThread.join();
        }

        if (m_maintenanceThread.joinable())
        {
            m_maintenanceThread.join();
        }
    }

    void IbkrMarketDataSource::websocketReceiveLoop()
    {
        while (m_running.load())
        {
            try
            {
                std::string message;
                if (!m_websocketClient->receiveText(message))
                {
                    break;
                }

                if (message.empty() || message == "tic")
                {
                    continue;
                }

                const auto quote = parseIbkrStreamingPayload(message);
                if (!quote.has_value())
                {
                    continue;
                }

                enqueueQuoteEvent(*quote);
            }
            catch (const std::exception& ex)
            {
                std::cerr << "[ibkr] websocket receive failed: "
                    << ex.what() << '\n';
                break;
            }
        }
    }

    void IbkrMarketDataSource::websocketMaintenanceLoop()
    {
        const auto pingInterval =
            std::chrono::milliseconds(m_config.websocketPingIntervalMs);
        auto nextPingTime = std::chrono::steady_clock::now() + pingInterval;

        while (m_running.load())
        {
            const auto now = std::chrono::steady_clock::now();
            if (now < nextPingTime)
            {
                std::this_thread::sleep_for(std::min(
                    std::chrono::milliseconds(100),
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        nextPingTime - now)));
                continue;
            }

            try
            {
                m_websocketClient->sendText("tic");
                bool authenticated = false;
                std::string sessionToken;
                std::string detail;
                if (!m_client->tickle(authenticated, sessionToken, detail) || !authenticated)
                {
                    std::cerr << "[ibkr] session keepalive failed: "
                        << detail << '\n';
                }
            }
            catch (const std::exception& ex)
            {
                std::cerr << "[ibkr] websocket keepalive failed: "
                    << ex.what() << '\n';
            }

            nextPingTime = std::chrono::steady_clock::now() + pingInterval;
        }
    }

    void IbkrMarketDataSource::enqueueQuoteEvent(const IbkrQuote& quote)
    {
        IbkrQuote& cachedQuote = m_cachedQuotesByConid[quote.conid];
        mergeIbkrQuote(cachedQuote, quote);

        const std::string symbol = resolveSymbol(cachedQuote.conid);
        if (symbol.empty())
        {
            return;
        }

        const SequenceNumber sequenceNumber = ++m_sequenceBySymbol[symbol];
        const auto mappedEvent =
            IbkrQuoteMapper::mapQuote(cachedQuote, symbol, sequenceNumber);
        if (!mappedEvent.has_value())
        {
            return;
        }

        std::lock_guard<std::mutex> lock(m_bufferMutex);
        if (m_buffer.size() >= m_config.eventQueueCapacity)
        {
            m_buffer.pop_front();
        }

        m_buffer.push_back(mappedEvent->event);
    }

    std::string IbkrMarketDataSource::resolveSymbol(const std::string& conid) const
    {
        const auto mappingIt = m_config.symbolsByConid.find(conid);
        if (mappingIt != m_config.symbolsByConid.end() &&
            !mappingIt->second.empty())
        {
            return mappingIt->second;
        }

        return conid;
    }

    std::string IbkrMarketDataSource::websocketUrl() const
    {
        if (!m_config.websocketUrl.empty())
        {
            return m_config.websocketUrl;
        }

        std::string url = trimTrailingSlash(m_config.baseUrl);
        if (url.rfind("https://", 0) == 0)
        {
            url.replace(0, 8, "wss://");
        }
        else if (url.rfind("http://", 0) == 0)
        {
            url.replace(0, 7, "ws://");
        }

        return url + "/ws";
    }

    IbkrMarketDataSource::Transport IbkrMarketDataSource::transport() const
    {
        return m_config.transport == "websocket"
            ? Transport::Websocket
            : Transport::Snapshot;
    }
}
