#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

namespace mdp::publisher::config
{
    struct RuntimeConfig
    {
        std::size_t eventCount{ 100000 };
        std::size_t burstSize{ 100 };
        std::uint32_t sleepUs{ 0 };
    };

    struct SourceConfig
    {
        std::string sourceType{ "synthetic" };
    };

    class PublisherConfig
    {
    public:
        static PublisherConfig loadFromIni(const std::filesystem::path& filePath);

        const RuntimeConfig& runtime() const { return m_runtime; }
        const SourceConfig& source() const { return m_source; }

    private:
        RuntimeConfig m_runtime;
        SourceConfig m_source;
    };
}
