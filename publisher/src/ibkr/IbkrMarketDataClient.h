#pragma once

#include "PublisherConfig.h"
#include "ibkr/IbkrQuote.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace mdp::publisher
{
    struct IbkrResolvedContract
    {
        std::string requestedSymbol;
        std::string symbol;
        std::string conid;
    };

    class IIbkrHttpClient
    {
    public:
        struct Response
        {
            int statusCode{ 0 };
            std::string body;
        };

        virtual ~IIbkrHttpClient() = default;
        virtual Response get(
            const std::string& url,
            bool allowInsecureLocalhostTls) = 0;
        virtual Response post(
            const std::string& url,
            const std::string& body,
            bool allowInsecureLocalhostTls) = 0;
    };

    class IIbkrMarketDataClient
    {
    public:
        virtual ~IIbkrMarketDataClient() = default;

        virtual bool checkAuthStatus(bool& authenticated, std::string& detail) = 0;
        virtual bool callAccounts(std::string& detail) = 0;
        virtual bool tickle(
            bool& authenticated,
            std::string& sessionToken,
            std::string& detail) = 0;
        virtual std::optional<IbkrResolvedContract> resolveContract(
            const std::string& symbol,
            const std::string& securityType) = 0;
        virtual std::vector<IbkrQuote> fetchSnapshot(
            const std::vector<std::string>& conids,
            const std::vector<std::string>& fields) = 0;
    };

    class WinHttpIbkrHttpClient final : public IIbkrHttpClient
    {
    public:
        Response get(
            const std::string& url,
            bool allowInsecureLocalhostTls) override;
        Response post(
            const std::string& url,
            const std::string& body,
            bool allowInsecureLocalhostTls) override;
    };

    class IbkrMarketDataClient final : public IIbkrMarketDataClient
    {
    public:
        IbkrMarketDataClient(
            config::IbkrConfig config,
            std::unique_ptr<IIbkrHttpClient> httpClient =
                std::make_unique<WinHttpIbkrHttpClient>());

        bool checkAuthStatus(bool& authenticated, std::string& detail) override;
        bool callAccounts(std::string& detail) override;
        bool tickle(
            bool& authenticated,
            std::string& sessionToken,
            std::string& detail) override;
        std::optional<IbkrResolvedContract> resolveContract(
            const std::string& symbol,
            const std::string& securityType) override;
        std::vector<IbkrQuote> fetchSnapshot(
            const std::vector<std::string>& conids,
            const std::vector<std::string>& fields) override;

    private:
        std::string buildUrl(const std::string& pathAndQuery) const;

    private:
        config::IbkrConfig m_config;
        std::unique_ptr<IIbkrHttpClient> m_httpClient;
    };
}
