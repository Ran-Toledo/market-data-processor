#pragma once

#include <memory>
#include <string>

namespace mdp::publisher
{
    class IIbkrWebSocketClient
    {
    public:
        virtual ~IIbkrWebSocketClient() = default;
        virtual void connect(
            const std::string& url,
            const std::string& sessionToken,
            bool allowInsecureLocalhostTls) = 0;
        virtual void sendText(const std::string& message) = 0;
        virtual bool receiveText(std::string& message) = 0;
        virtual void close() = 0;
    };

    class WinHttpIbkrWebSocketClient final : public IIbkrWebSocketClient
    {
    public:
        WinHttpIbkrWebSocketClient();
        ~WinHttpIbkrWebSocketClient() override;

        void connect(
            const std::string& url,
            const std::string& sessionToken,
            bool allowInsecureLocalhostTls) override;
        void sendText(const std::string& message) override;
        bool receiveText(std::string& message) override;
        void close() override;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
