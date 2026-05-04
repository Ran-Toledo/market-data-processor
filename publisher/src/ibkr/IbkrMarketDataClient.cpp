#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "ibkr/IbkrMarketDataClient.h"

#include "ibkr/IbkrQuoteParser.h"
#include "ibkr/SimpleJson.h"

#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <cctype>
#include <optional>
#include <stdexcept>

#pragma comment(lib, "winhttp.lib")

namespace mdp::publisher
{
    namespace
    {
        struct ParsedUrl
        {
            std::wstring host;
            std::wstring pathAndQuery;
            INTERNET_PORT port{ 0 };
            bool secure{ false };
            bool localhost{ false };
        };

        class WinHttpHandle
        {
        public:
            explicit WinHttpHandle(HINTERNET handle = nullptr)
                : m_handle(handle)
            {
            }

            ~WinHttpHandle()
            {
                if (m_handle != nullptr)
                {
                    WinHttpCloseHandle(m_handle);
                }
            }

            WinHttpHandle(const WinHttpHandle&) = delete;
            WinHttpHandle& operator=(const WinHttpHandle&) = delete;

            WinHttpHandle(WinHttpHandle&& other) noexcept
                : m_handle(other.m_handle)
            {
                other.m_handle = nullptr;
            }

            WinHttpHandle& operator=(WinHttpHandle&& other) noexcept
            {
                if (this != &other)
                {
                    if (m_handle != nullptr)
                    {
                        WinHttpCloseHandle(m_handle);
                    }

                    m_handle = other.m_handle;
                    other.m_handle = nullptr;
                }

                return *this;
            }

            HINTERNET get() const { return m_handle; }

        private:
            HINTERNET m_handle{ nullptr };
        };

        std::wstring widen(const std::string& text)
        {
            if (text.empty())
            {
                return {};
            }

            const int required = MultiByteToWideChar(
                CP_UTF8,
                0,
                text.c_str(),
                static_cast<int>(text.size()),
                nullptr,
                0);
            if (required <= 0)
            {
                throw std::runtime_error("Failed to convert string to UTF-16");
            }

            std::wstring result(static_cast<std::size_t>(required), L'\0');
            MultiByteToWideChar(
                CP_UTF8,
                0,
                text.c_str(),
                static_cast<int>(text.size()),
                result.data(),
                required);
            return result;
        }

        std::string narrow(const std::wstring& text)
        {
            if (text.empty())
            {
                return {};
            }

            const int required = WideCharToMultiByte(
                CP_UTF8,
                0,
                text.c_str(),
                static_cast<int>(text.size()),
                nullptr,
                0,
                nullptr,
                nullptr);
            if (required <= 0)
            {
                throw std::runtime_error("Failed to convert string to UTF-8");
            }

            std::string result(static_cast<std::size_t>(required), '\0');
            WideCharToMultiByte(
                CP_UTF8,
                0,
                text.c_str(),
                static_cast<int>(text.size()),
                result.data(),
                required,
                nullptr,
                nullptr);
            return result;
        }

        std::string trimTrailingSlash(std::string value)
        {
            while (!value.empty() && value.back() == '/')
            {
                value.pop_back();
            }

            return value;
        }

        std::string urlEncode(const std::string& value)
        {
            static constexpr char hexDigits[] = "0123456789ABCDEF";

            std::string encoded;
            encoded.reserve(value.size());
            for (unsigned char c : value)
            {
                if (std::isalnum(c) != 0 || c == '-' || c == '_' || c == '.' || c == '~')
                {
                    encoded.push_back(static_cast<char>(c));
                    continue;
                }

                encoded.push_back('%');
                encoded.push_back(hexDigits[(c >> 4) & 0x0F]);
                encoded.push_back(hexDigits[c & 0x0F]);
            }

            return encoded;
        }

        ParsedUrl parseUrl(const std::string& url)
        {
            URL_COMPONENTS components{};
            components.dwStructSize = sizeof(components);
            components.dwSchemeLength = static_cast<DWORD>(-1);
            components.dwHostNameLength = static_cast<DWORD>(-1);
            components.dwUrlPathLength = static_cast<DWORD>(-1);
            components.dwExtraInfoLength = static_cast<DWORD>(-1);

            const std::wstring wideUrl = widen(url);
            if (!WinHttpCrackUrl(
                wideUrl.c_str(),
                static_cast<DWORD>(wideUrl.size()),
                0,
                &components))
            {
                throw std::runtime_error("Failed to parse IBKR URL: " + url);
            }

            ParsedUrl parsed;
            parsed.host.assign(components.lpszHostName, components.dwHostNameLength);
            parsed.port = components.nPort;
            parsed.secure = components.nScheme == INTERNET_SCHEME_HTTPS;
            std::wstring path(components.lpszUrlPath, components.dwUrlPathLength);
            std::wstring extra(components.lpszExtraInfo, components.dwExtraInfoLength);
            parsed.pathAndQuery = path + extra;

            const std::string hostLower = [&parsed]()
            {
                std::string lower = narrow(parsed.host);
                std::transform(
                    lower.begin(),
                    lower.end(),
                    lower.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                return lower;
            }();
            parsed.localhost = hostLower == "localhost" || hostLower == "127.0.0.1";
            return parsed;
        }

        IIbkrHttpClient::Response sendRequest(
            const std::string& method,
            const std::string& url,
            const std::string& body,
            bool allowInsecureLocalhostTls)
        {
            const ParsedUrl parsed = parseUrl(url);

            WinHttpHandle session(WinHttpOpen(
                L"market_data_publisher/1.0",
                WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                WINHTTP_NO_PROXY_NAME,
                WINHTTP_NO_PROXY_BYPASS,
                0));
            if (session.get() == nullptr)
            {
                throw std::runtime_error("WinHttpOpen failed");
            }

            WinHttpHandle connection(WinHttpConnect(
                session.get(),
                parsed.host.c_str(),
                parsed.port,
                0));
            if (connection.get() == nullptr)
            {
                throw std::runtime_error("WinHttpConnect failed");
            }

            const DWORD flags = parsed.secure ? WINHTTP_FLAG_SECURE : 0;
            const std::wstring wideMethod = widen(method);
            WinHttpHandle request(WinHttpOpenRequest(
                connection.get(),
                wideMethod.c_str(),
                parsed.pathAndQuery.c_str(),
                nullptr,
                WINHTTP_NO_REFERER,
                WINHTTP_DEFAULT_ACCEPT_TYPES,
                flags));
            if (request.get() == nullptr)
            {
                throw std::runtime_error("WinHttpOpenRequest failed");
            }

            if (parsed.secure && allowInsecureLocalhostTls && parsed.localhost)
            {
                const DWORD securityFlags =
                    SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                    SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                    SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                    SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
                WinHttpSetOption(
                    request.get(),
                    WINHTTP_OPTION_SECURITY_FLAGS,
                    const_cast<DWORD*>(&securityFlags),
                    sizeof(securityFlags));
            }

            const std::wstring extraHeaders = body.empty()
                ? L"Accept: application/json\r\n"
                : L"Accept: application/json\r\nContent-Type: application/json\r\n";
            LPVOID bodyPtr = body.empty()
                ? WINHTTP_NO_REQUEST_DATA
                : const_cast<char*>(body.data());
            const DWORD bodySize = static_cast<DWORD>(body.size());

            if (!WinHttpSendRequest(
                request.get(),
                extraHeaders.c_str(),
                static_cast<DWORD>(-1L),
                bodyPtr,
                bodySize,
                bodySize,
                0))
            {
                throw std::runtime_error("WinHttpSendRequest failed");
            }

            if (!WinHttpReceiveResponse(request.get(), nullptr))
            {
                throw std::runtime_error("WinHttpReceiveResponse failed");
            }

            DWORD statusCode = 0;
            DWORD statusCodeSize = sizeof(statusCode);
            if (!WinHttpQueryHeaders(
                request.get(),
                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX,
                &statusCode,
                &statusCodeSize,
                WINHTTP_NO_HEADER_INDEX))
            {
                throw std::runtime_error("WinHttpQueryHeaders failed");
            }

            std::string responseBody;
            while (true)
            {
                DWORD availableSize = 0;
                if (!WinHttpQueryDataAvailable(request.get(), &availableSize))
                {
                    throw std::runtime_error("WinHttpQueryDataAvailable failed");
                }

                if (availableSize == 0)
                {
                    break;
                }

                std::string chunk(static_cast<std::size_t>(availableSize), '\0');
                DWORD bytesRead = 0;
                if (!WinHttpReadData(
                    request.get(),
                    chunk.data(),
                    availableSize,
                    &bytesRead))
                {
                    throw std::runtime_error("WinHttpReadData failed");
                }

                chunk.resize(static_cast<std::size_t>(bytesRead));
                responseBody += chunk;
            }

            return IIbkrHttpClient::Response{ static_cast<int>(statusCode), responseBody };
        }

        std::optional<bool> tryGetBool(
            const json::Value::Object& object,
            const std::string& key)
        {
            const auto it = object.find(key);
            if (it == object.end())
            {
                return std::nullopt;
            }

            if (it->second.isBoolean())
            {
                return it->second.asBoolean();
            }

            if (it->second.isString())
            {
                std::string text = it->second.asString();
                std::transform(
                    text.begin(),
                    text.end(),
                    text.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (text == "true")
                {
                    return true;
                }
                if (text == "false")
                {
                    return false;
                }
            }

            return std::nullopt;
        }

        std::optional<std::string> tryGetString(
            const json::Value::Object& object,
            const std::string& key)
        {
            const auto it = object.find(key);
            if (it == object.end())
            {
                return std::nullopt;
            }

            if (it->second.isString())
            {
                return it->second.asString();
            }

            return std::nullopt;
        }

        std::optional<std::string> tryGetStringOrNumber(
            const json::Value::Object& object,
            const std::string& key)
        {
            const auto it = object.find(key);
            if (it == object.end())
            {
                return std::nullopt;
            }

            if (it->second.isString())
            {
                return it->second.asString();
            }

            if (it->second.isNumber())
            {
                const double value = it->second.asNumber();
                const auto wholeValue = static_cast<long long>(value);
                if (value == static_cast<double>(wholeValue))
                {
                    return std::to_string(wholeValue);
                }

                return std::to_string(value);
            }

            return std::nullopt;
        }

        bool equalsIgnoreCase(const std::string& lhs, const std::string& rhs)
        {
            if (lhs.size() != rhs.size())
            {
                return false;
            }

            for (std::size_t i = 0; i < lhs.size(); ++i)
            {
                if (std::tolower(static_cast<unsigned char>(lhs[i])) !=
                    std::tolower(static_cast<unsigned char>(rhs[i])))
                {
                    return false;
                }
            }

            return true;
        }
    }

    IIbkrHttpClient::Response WinHttpIbkrHttpClient::get(
        const std::string& url,
        bool allowInsecureLocalhostTls)
    {
        return sendRequest("GET", url, "", allowInsecureLocalhostTls);
    }

    IIbkrHttpClient::Response WinHttpIbkrHttpClient::post(
        const std::string& url,
        const std::string& body,
        bool allowInsecureLocalhostTls)
    {
        return sendRequest("POST", url, body, allowInsecureLocalhostTls);
    }

    IbkrMarketDataClient::IbkrMarketDataClient(
        config::IbkrConfig config,
        std::unique_ptr<IIbkrHttpClient> httpClient)
        : m_config(std::move(config))
        , m_httpClient(std::move(httpClient))
    {
    }

    bool IbkrMarketDataClient::checkAuthStatus(bool& authenticated, std::string& detail)
    {
        const auto response = m_httpClient->get(
            buildUrl("/iserver/auth/status"),
            m_config.allowInsecureLocalhostTls);

        if (response.statusCode != 200)
        {
            detail = "HTTP " + std::to_string(response.statusCode);
            authenticated = false;
            return false;
        }

        const json::Value payload = json::parse(response.body);
        const auto& object = payload.asObject();
        authenticated = tryGetBool(object, "authenticated").value_or(false);
        detail = authenticated ? "authenticated" : "not authenticated";
        return true;
    }

    bool IbkrMarketDataClient::callAccounts(std::string& detail)
    {
        const auto response = m_httpClient->get(
            buildUrl("/iserver/accounts"),
            m_config.allowInsecureLocalhostTls);

        detail = "HTTP " + std::to_string(response.statusCode);
        return response.statusCode == 200;
    }

    bool IbkrMarketDataClient::tickle(
        bool& authenticated,
        std::string& sessionToken,
        std::string& detail)
    {
        const auto response = m_httpClient->post(
            buildUrl("/tickle"),
            "{}",
            m_config.allowInsecureLocalhostTls);

        if (response.statusCode != 200)
        {
            authenticated = false;
            sessionToken.clear();
            detail = "HTTP " + std::to_string(response.statusCode);
            return false;
        }

        const json::Value payload = json::parse(response.body);
        const auto& object = payload.asObject();
        authenticated = tryGetBool(object, "iserver").value_or(false);

        const auto session = tryGetString(object, "session");
        sessionToken = session.value_or("");
        detail = authenticated ? "authenticated" : "not authenticated";
        return true;
    }

    std::optional<IbkrResolvedContract> IbkrMarketDataClient::resolveContract(
        const std::string& symbol,
        const std::string& securityType)
    {
        const auto response = m_httpClient->get(
            buildUrl(
                "/iserver/secdef/search?symbol=" + urlEncode(symbol) +
                "&secType=" + urlEncode(securityType)),
            m_config.allowInsecureLocalhostTls);

        if (response.statusCode != 200)
        {
            throw std::runtime_error(
                "IBKR contract search failed for symbol " + symbol +
                " with HTTP " + std::to_string(response.statusCode));
        }

        const json::Value payload = json::parse(response.body);
        if (!payload.isArray())
        {
            throw std::runtime_error(
                "IBKR contract search returned unexpected JSON for symbol " + symbol);
        }

        std::optional<IbkrResolvedContract> fallback;
        for (const auto& item : payload.asArray())
        {
            if (!item.isObject())
            {
                continue;
            }

            const auto& object = item.asObject();
            const auto conid = tryGetStringOrNumber(object, "conid");
            if (!conid.has_value() || conid->empty())
            {
                continue;
            }

            const std::string resolvedSymbol =
                tryGetString(object, "symbol").value_or(symbol);
            IbkrResolvedContract contract{
                symbol,
                resolvedSymbol,
                *conid
            };

            if (equalsIgnoreCase(resolvedSymbol, symbol))
            {
                return contract;
            }

            if (!fallback.has_value())
            {
                fallback = contract;
            }
        }

        return fallback;
    }

    std::vector<IbkrQuote> IbkrMarketDataClient::fetchSnapshot(
        const std::vector<std::string>& conids,
        const std::vector<std::string>& fields)
    {
        if (conids.empty())
        {
            return {};
        }

        std::string query = "/iserver/marketdata/snapshot?conids=";
        for (std::size_t i = 0; i < conids.size(); ++i)
        {
            if (i > 0)
            {
                query += ',';
            }
            query += conids[i];
        }
        query += "&fields=";
        for (std::size_t i = 0; i < fields.size(); ++i)
        {
            if (i > 0)
            {
                query += ',';
            }
            query += fields[i];
        }

        const auto response = m_httpClient->get(
            buildUrl(query),
            m_config.allowInsecureLocalhostTls);

        if (response.statusCode != 200)
        {
            throw std::runtime_error(
                "IBKR snapshot request failed with HTTP " +
                std::to_string(response.statusCode));
        }

        return parseIbkrSnapshotPayload(response.body);
    }

    std::string IbkrMarketDataClient::buildUrl(const std::string& pathAndQuery) const
    {
        return trimTrailingSlash(m_config.baseUrl) + pathAndQuery;
    }
}
