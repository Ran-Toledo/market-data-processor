#include "PublisherConfig.h"
#include "ibkr/IbkrMarketDataClient.h"
#include "ibkr/IbkrMarketDataSource.h"
#include "ibkr/IbkrQuoteMapper.h"
#include "ibkr/IbkrWebSocketClient.h"

#include <cassert>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace
{
    class FakeIbkrMarketDataClient final : public mdp::publisher::IIbkrMarketDataClient
    {
    public:
        bool checkAuthStatus(bool& authenticated, std::string& detail) override
        {
            ++authCalls;
            authenticated = authOk;
            detail = authOk ? "authenticated" : "not authenticated";
            return authRequestOk;
        }

        bool callAccounts(std::string& detail) override
        {
            ++accountsCalls;
            detail = accountsOk ? "HTTP 200" : "HTTP 500";
            return accountsOk;
        }

        bool tickle(
            bool& authenticated,
            std::string& sessionToken,
            std::string& detail) override
        {
            ++tickleCalls;
            authenticated = authOk;
            sessionToken = "session-token";
            detail = authOk ? "authenticated" : "not authenticated";
            return authRequestOk;
        }

        std::optional<mdp::publisher::IbkrResolvedContract> resolveContract(
            const std::string& symbol,
            const std::string&) override
        {
            ++resolveCalls;
            for (const auto& contract : resolvedContracts)
            {
                if (contract.requestedSymbol == symbol)
                {
                    return contract;
                }
            }

            return std::nullopt;
        }

        std::vector<mdp::publisher::IbkrQuote> fetchSnapshot(
            const std::vector<std::string>&,
            const std::vector<std::string>&) override
        {
            ++snapshotCalls;
            return quotes;
        }

        bool authRequestOk{ true };
        bool authOk{ true };
        bool accountsOk{ true };
        int authCalls{ 0 };
        int accountsCalls{ 0 };
        int tickleCalls{ 0 };
        int resolveCalls{ 0 };
        int snapshotCalls{ 0 };
        std::vector<mdp::publisher::IbkrResolvedContract> resolvedContracts;
        std::vector<mdp::publisher::IbkrQuote> quotes;
    };

    class FakeIbkrWebSocketClient final : public mdp::publisher::IIbkrWebSocketClient
    {
    public:
        void connect(
            const std::string&,
            const std::string&,
            bool) override
        {
            connected = true;
        }

        void sendText(const std::string& message) override
        {
            sentMessages.push_back(message);
        }

        bool receiveText(std::string& message) override
        {
            if (messages.empty())
            {
                return false;
            }

            message = messages.front();
            messages.erase(messages.begin());
            return true;
        }

        void close() override
        {
            closed = true;
        }

        bool connected{ false };
        bool closed{ false };
        std::vector<std::string> sentMessages;
        std::vector<std::string> messages;
    };

    mdp::publisher::config::IbkrConfig makeIbkrConfig()
    {
        mdp::publisher::config::IbkrConfig config;
        config.conids = { "265598", "8314" };
        config.symbols = { "AAPL", "IBM" };
        config.securityType = "STK";
        config.symbolsByConid = {
            { "265598", "AAPL" },
            { "8314", "IBM" }
        };
        config.pollIntervalMs = 1000;
        config.checkAuthOnStartup = true;
        config.callAccountsOnStartup = true;
        return config;
    }

    void testMapperUsesLastPrice()
    {
        mdp::publisher::IbkrQuote quote;
        quote.conid = "265598";
        quote.lastPrice = 187.25;
        quote.bidSize = 25;
        quote.updatedMs = 1710000000123ULL;

        const auto mapped =
            mdp::publisher::IbkrQuoteMapper::mapQuote(quote, "AAPL", 1);
        assert(mapped.has_value());
        assert(mapped->event.symbol == "AAPL");
        assert(mapped->event.price == 187.25);
        assert(mapped->event.volume == 25);
        assert(mapped->event.exchangeTimestampNs == 1710000000123000000ULL);
        assert(mapped->event.sequenceNumber == 1);
        assert(!mapped->usedMidpoint);
        assert(mapped->usedQuoteSize);
    }

    void testMapperFallsBackToMidpoint()
    {
        mdp::publisher::IbkrQuote quote;
        quote.conid = "8314";
        quote.bidPrice = 99.0;
        quote.askPrice = 101.0;

        const auto mapped =
            mdp::publisher::IbkrQuoteMapper::mapQuote(quote, "IBM", 7);
        assert(mapped.has_value());
        assert(mapped->event.price == 100.0);
        assert(mapped->usedMidpoint);
    }

    void testMapperSkipsMissingPrice()
    {
        mdp::publisher::IbkrQuote quote;
        quote.conid = "8314";
        quote.bidSize = 10;
        quote.askSize = 12;

        const auto mapped =
            mdp::publisher::IbkrQuoteMapper::mapQuote(quote, "IBM", 3);
        assert(!mapped.has_value());
    }

    void testSourceAssignsPerSymbolSequenceAndMapping()
    {
        auto fakeClient = std::make_unique<FakeIbkrMarketDataClient>();
        fakeClient->resolvedContracts = {
            { "AAPL", "AAPL", "265598" },
            { "IBM", "IBM", "8314" }
        };
        fakeClient->quotes = {
            mdp::publisher::IbkrQuote{ "265598", 185.5, std::nullopt, std::nullopt, 10, 12, 1000ULL },
            mdp::publisher::IbkrQuote{ "265598", 185.7, std::nullopt, std::nullopt, 11, 13, 1001ULL },
            mdp::publisher::IbkrQuote{ "8314", 99.1, std::nullopt, std::nullopt, 5, 6, 1002ULL }
        };
        FakeIbkrMarketDataClient* fakeClientPtr = fakeClient.get();

        mdp::publisher::IbkrMarketDataSource source(
            makeIbkrConfig(),
            std::move(fakeClient));

        mdp::MarketDataEvent first;
        mdp::MarketDataEvent second;
        mdp::MarketDataEvent third;
        assert(source.next(first));
        assert(source.next(second));
        assert(source.next(third));
        assert(first.symbol == "AAPL");
        assert(second.symbol == "AAPL");
        assert(third.symbol == "IBM");
        assert(first.sequenceNumber == 1);
        assert(second.sequenceNumber == 2);
        assert(third.sequenceNumber == 1);
        assert(fakeClientPtr->authCalls == 1);
        assert(fakeClientPtr->accountsCalls == 1);
        assert(fakeClientPtr->resolveCalls == 2);
        assert(fakeClientPtr->snapshotCalls == 1);
    }

    void testSourceUsesConidAsFallbackSymbol()
    {
        auto fakeClient = std::make_unique<FakeIbkrMarketDataClient>();
        fakeClient->resolvedContracts = {
            { "AAPL", "AAPL", "265598" },
            { "IBM", "IBM", "8314" }
        };
        fakeClient->quotes = {
            mdp::publisher::IbkrQuote{ "9999", 12.5, std::nullopt, std::nullopt, 1, 1, 1000ULL }
        };

        auto config = makeIbkrConfig();
        config.symbolsByConid.clear();

        mdp::publisher::IbkrMarketDataSource source(
            config,
            std::move(fakeClient));

        mdp::MarketDataEvent event;
        assert(source.next(event));
        assert(event.symbol == "9999");
        assert(event.sequenceNumber == 1);
    }

    void testSourceResolvesConfiguredSymbolsToConids()
    {
        auto fakeClient = std::make_unique<FakeIbkrMarketDataClient>();
        fakeClient->resolvedContracts = {
            { "AAPL", "AAPL", "265598" },
            { "IBM", "IBM", "8314" }
        };
        fakeClient->quotes = {
            mdp::publisher::IbkrQuote{ "265598", 185.0, std::nullopt, std::nullopt, 4, 4, 1000ULL },
            mdp::publisher::IbkrQuote{ "8314", 99.0, std::nullopt, std::nullopt, 5, 5, 1001ULL }
        };

        auto config = makeIbkrConfig();
        config.conids.clear();
        config.symbolsByConid.clear();

        mdp::publisher::IbkrMarketDataSource source(
            config,
            std::move(fakeClient));

        mdp::MarketDataEvent first;
        mdp::MarketDataEvent second;
        assert(source.next(first));
        assert(source.next(second));
        assert(first.symbol == "AAPL");
        assert(second.symbol == "IBM");
    }

    void testWebsocketSourceConsumesStreamingUpdate()
    {
        auto fakeClient = std::make_unique<FakeIbkrMarketDataClient>();
        fakeClient->resolvedContracts = {
            { "AAPL", "AAPL", "265598" },
            { "IBM", "IBM", "8314" }
        };
        auto fakeWebSocket = std::make_unique<FakeIbkrWebSocketClient>();
        fakeWebSocket->messages = {
            R"({"topic":"smd+265598","conid":"265598","31":"190.5","85":"7","_updated":"1710000000456"})"
        };
        FakeIbkrWebSocketClient* fakeWebSocketPtr = fakeWebSocket.get();
        auto config = makeIbkrConfig();
        config.transport = "websocket";
        config.websocketPingIntervalMs = 60000;

        mdp::publisher::IbkrMarketDataSource source(
            config,
            std::move(fakeClient),
            std::move(fakeWebSocket));

        mdp::MarketDataEvent event;
        for (int i = 0; i < 50; ++i)
        {
            if (source.next(event))
            {
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        assert(event.symbol == "AAPL");
        assert(event.price == 190.5);
        assert(event.volume == 7);
        assert(event.sequenceNumber == 1);
        assert(fakeWebSocketPtr->connected);
        assert(!fakeWebSocketPtr->sentMessages.empty());
    }
}

void runIbkrMarketDataSourceTests()
{
    testMapperUsesLastPrice();
    testMapperFallsBackToMidpoint();
    testMapperSkipsMissingPrice();
    testSourceAssignsPerSymbolSequenceAndMapping();
    testSourceUsesConidAsFallbackSymbol();
    testSourceResolvesConfiguredSymbolsToConids();
    testWebsocketSourceConsumesStreamingUpdate();
}
