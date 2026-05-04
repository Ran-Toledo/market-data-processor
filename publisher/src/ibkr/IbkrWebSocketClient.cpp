#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "ibkr/IbkrWebSocketClient.h"

#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <vector>

#pragma comment(lib, "winhttp.lib")

namespace mdp::publisher
{
    namespace
    {
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

            HINTERNET get() const { return m_handle; }
            HINTERNET release()
            {
                HINTERNET value = m_handle;
                m_handle = nullptr;
                return value;
            }

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

        struct ParsedUrl
        {
            std::wstring host;
            std::wstring pathAndQuery;
            INTERNET_PORT port{ 0 };
            bool secure{ false };
            bool localhost{ false };
        };

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
                throw std::runtime_error("Failed to parse websocket URL: " + url);
            }

            ParsedUrl parsed;
            parsed.host.assign(components.lpszHostName, components.dwHostNameLength);
            parsed.port = components.nPort;
            parsed.secure = components.nScheme == INTERNET_SCHEME_HTTPS;
            parsed.pathAndQuery.assign(components.lpszUrlPath, components.dwUrlPathLength);
            parsed.pathAndQuery.append(components.lpszExtraInfo, components.dwExtraInfoLength);

            std::string hostLower = narrow(parsed.host);
            std::transform(
                hostLower.begin(),
                hostLower.end(),
                hostLower.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            parsed.localhost = hostLower == "localhost" || hostLower == "127.0.0.1";
            return parsed;
        }
    }

    struct WinHttpIbkrWebSocketClient::Impl
    {
        HINTERNET session{ nullptr };
        HINTERNET connection{ nullptr };
        HINTERNET request{ nullptr };
        HINTERNET websocket{ nullptr };
    };

    WinHttpIbkrWebSocketClient::WinHttpIbkrWebSocketClient()
        : m_impl(std::make_unique<Impl>())
    {
    }

    WinHttpIbkrWebSocketClient::~WinHttpIbkrWebSocketClient()
    {
        close();
    }

    void WinHttpIbkrWebSocketClient::connect(
        const std::string& url,
        const std::string& sessionToken,
        bool allowInsecureLocalhostTls)
    {
        close();

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
        WinHttpHandle request(WinHttpOpenRequest(
            connection.get(),
            L"GET",
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

        if (!WinHttpSetOption(
            request.get(),
            WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET,
            nullptr,
            0))
        {
            throw std::runtime_error("Failed to enable websocket upgrade");
        }

        const std::wstring headers = widen(
            "Cookie: api=" + sessionToken + "\r\nOrigin: https://localhost:5000\r\n");
        if (!WinHttpSendRequest(
            request.get(),
            headers.c_str(),
            static_cast<DWORD>(-1L),
            WINHTTP_NO_REQUEST_DATA,
            0,
            0,
            0))
        {
            throw std::runtime_error("WinHttpSendRequest failed");
        }

        if (!WinHttpReceiveResponse(request.get(), nullptr))
        {
            throw std::runtime_error("WinHttpReceiveResponse failed");
        }

        HINTERNET websocket = WinHttpWebSocketCompleteUpgrade(request.get(), 0);
        if (websocket == nullptr)
        {
            throw std::runtime_error("WinHttpWebSocketCompleteUpgrade failed");
        }

        m_impl->session = session.release();
        m_impl->connection = connection.release();
        m_impl->request = request.release();
        m_impl->websocket = websocket;
    }

    void WinHttpIbkrWebSocketClient::sendText(const std::string& message)
    {
        if (m_impl->websocket == nullptr)
        {
            throw std::runtime_error("Websocket is not connected");
        }

        const DWORD result = WinHttpWebSocketSend(
            m_impl->websocket,
            WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
            static_cast<PVOID>(const_cast<char*>(message.data())),
            static_cast<DWORD>(message.size()));
        if (result != NO_ERROR)
        {
            throw std::runtime_error("WinHttpWebSocketSend failed");
        }
    }

    bool WinHttpIbkrWebSocketClient::receiveText(std::string& message)
    {
        message.clear();

        if (m_impl->websocket == nullptr)
        {
            return false;
        }

        std::vector<char> buffer(4096);
        WINHTTP_WEB_SOCKET_BUFFER_TYPE bufferType =
            WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE;

        while (true)
        {
            DWORD bytesRead = 0;
            const DWORD result = WinHttpWebSocketReceive(
                m_impl->websocket,
                buffer.data(),
                static_cast<DWORD>(buffer.size()),
                &bytesRead,
                &bufferType);
            if (result != NO_ERROR)
            {
                return false;
            }

            if (bufferType == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE)
            {
                return false;
            }

            message.append(buffer.data(), buffer.data() + bytesRead);

            if (bufferType == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE ||
                bufferType == WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE)
            {
                return true;
            }
        }
    }

    void WinHttpIbkrWebSocketClient::close()
    {
        if (m_impl->websocket != nullptr)
        {
            WinHttpWebSocketClose(
                m_impl->websocket,
                WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS,
                nullptr,
                0);
            WinHttpCloseHandle(m_impl->websocket);
            m_impl->websocket = nullptr;
        }

        if (m_impl->request != nullptr)
        {
            WinHttpCloseHandle(m_impl->request);
            m_impl->request = nullptr;
        }

        if (m_impl->connection != nullptr)
        {
            WinHttpCloseHandle(m_impl->connection);
            m_impl->connection = nullptr;
        }

        if (m_impl->session != nullptr)
        {
            WinHttpCloseHandle(m_impl->session);
            m_impl->session = nullptr;
        }
    }
}
