#include "PublisherConfig.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace mdp::publisher::config
{
    namespace
    {
        std::string trim(const std::string& value)
        {
            const auto begin = std::find_if_not(
                value.begin(),
                value.end(),
                [](unsigned char c) { return std::isspace(c) != 0; });
            const auto end = std::find_if_not(
                value.rbegin(),
                value.rend(),
                [](unsigned char c) { return std::isspace(c) != 0; }).base();

            if (begin >= end)
            {
                return {};
            }

            return std::string(begin, end);
        }

        std::string toLower(std::string value)
        {
            std::transform(
                value.begin(),
                value.end(),
                value.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return value;
        }

        bool parseBool(const std::string& value)
        {
            const std::string normalized = toLower(trim(value));

            if (normalized == "true" || normalized == "1" ||
                normalized == "yes" || normalized == "on")
            {
                return true;
            }

            if (normalized == "false" || normalized == "0" ||
                normalized == "no" || normalized == "off")
            {
                return false;
            }

            throw std::runtime_error("Invalid publisher boolean value: " + value);
        }

        std::vector<std::string> parseCommaSeparated(const std::string& value)
        {
            std::vector<std::string> items;
            std::stringstream stream(value);
            std::string item;

            while (std::getline(stream, item, ','))
            {
                const std::string trimmed = trim(item);
                if (!trimmed.empty())
                {
                    items.push_back(trimmed);
                }
            }

            return items;
        }
    }

    PublisherConfig PublisherConfig::loadFromIni(const std::filesystem::path& filePath)
    {
        std::ifstream input(filePath);
        if (!input.is_open())
        {
            throw std::runtime_error(
                "Failed to open publisher config file: " + filePath.string());
        }

        PublisherConfig config;
        std::string currentSection;
        std::string line;
        std::size_t lineNumber = 0;

        while (std::getline(input, line))
        {
            ++lineNumber;

            const std::string trimmedLine = trim(line);
            if (trimmedLine.empty() || trimmedLine[0] == '#' || trimmedLine[0] == ';')
            {
                continue;
            }

            if (trimmedLine.front() == '[' && trimmedLine.back() == ']')
            {
                currentSection = toLower(
                    trim(trimmedLine.substr(1, trimmedLine.size() - 2)));
                continue;
            }

            const std::size_t separator = trimmedLine.find('=');
            if (separator == std::string::npos)
            {
                throw std::runtime_error(
                    "Invalid publisher config line " + std::to_string(lineNumber) +
                    ": expected key=value");
            }

            const std::string key = toLower(trim(trimmedLine.substr(0, separator)));
            const std::string value = trim(trimmedLine.substr(separator + 1));

            if (currentSection == "runtime")
            {
                if (key == "event_count")
                {
                    config.m_runtime.eventCount =
                        static_cast<std::size_t>(std::stoull(value));
                }
                else if (key == "runtime_seconds")
                {
                    config.m_runtime.runtimeSeconds =
                        static_cast<std::uint32_t>(std::stoul(value));
                }
                else if (key == "burst_size")
                {
                    config.m_runtime.burstSize =
                        static_cast<std::size_t>(std::stoull(value));
                }
                else if (key == "sleep_us")
                {
                    config.m_runtime.sleepUs =
                        static_cast<std::uint32_t>(std::stoul(value));
                }
                else
                {
                    throw std::runtime_error(
                        "Unknown publisher runtime key on line " +
                        std::to_string(lineNumber) + ": " + key);
                }
            }
            else if (currentSection == "network")
            {
                if (key == "processor_host")
                {
                    config.m_network.processorHost = value;
                }
                else if (key == "processor_port")
                {
                    config.m_network.processorPort =
                        static_cast<std::uint16_t>(std::stoul(value));
                }
                else if (key == "connect_retry_ms")
                {
                    config.m_network.connectRetryMs =
                        static_cast<std::uint32_t>(std::stoul(value));
                }
                else if (key == "ack_window_batches")
                {
                    config.m_network.ackWindowBatches =
                        std::max<std::size_t>(
                            1,
                            static_cast<std::size_t>(std::stoull(value)));
                }
                else
                {
                    throw std::runtime_error(
                        "Unknown publisher network key on line " +
                        std::to_string(lineNumber) + ": " + key);
                }
            }
            else if (currentSection == "source")
            {
                if (key == "source_type")
                {
                    config.m_source.sourceType = toLower(value);
                }
                else if (key == "symbol_count")
                {
                    config.m_source.symbolCount =
                        static_cast<std::size_t>(std::stoull(value));
                }
                else if (key == "symbol_offset")
                {
                    config.m_source.symbolOffset =
                        static_cast<std::size_t>(std::stoull(value));
                }
                else
                {
                    throw std::runtime_error(
                        "Unknown publisher source key on line " +
                        std::to_string(lineNumber) + ": " + key);
                }
            }
            else if (currentSection == "ibkr")
            {
                if (key == "transport")
                {
                    config.m_ibkr.transport = toLower(value);
                }
                else if (key == "base_url")
                {
                    config.m_ibkr.baseUrl = value;
                }
                else if (key == "websocket_url")
                {
                    config.m_ibkr.websocketUrl = value;
                }
                else if (key == "symbols")
                {
                    config.m_ibkr.symbols = parseCommaSeparated(value);
                }
                else if (key == "security_type")
                {
                    config.m_ibkr.securityType = toLower(value);
                    std::transform(
                        config.m_ibkr.securityType.begin(),
                        config.m_ibkr.securityType.end(),
                        config.m_ibkr.securityType.begin(),
                        [](unsigned char c)
                        {
                            return static_cast<char>(std::toupper(c));
                        });
                }
                else if (key == "conids")
                {
                    config.m_ibkr.conids = parseCommaSeparated(value);
                }
                else if (key == "fields")
                {
                    config.m_ibkr.fields = parseCommaSeparated(value);
                }
                else if (key == "poll_interval_ms")
                {
                    config.m_ibkr.pollIntervalMs =
                        static_cast<std::uint32_t>(std::stoul(value));
                }
                else if (key == "websocket_ping_interval_ms")
                {
                    config.m_ibkr.websocketPingIntervalMs =
                        static_cast<std::uint32_t>(std::stoul(value));
                }
                else if (key == "event_queue_capacity")
                {
                    config.m_ibkr.eventQueueCapacity =
                        static_cast<std::size_t>(std::stoull(value));
                }
                else if (key == "check_auth_on_startup")
                {
                    config.m_ibkr.checkAuthOnStartup = parseBool(value);
                }
                else if (key == "call_accounts_on_startup")
                {
                    config.m_ibkr.callAccountsOnStartup = parseBool(value);
                }
                else if (key == "allow_insecure_localhost_tls")
                {
                    config.m_ibkr.allowInsecureLocalhostTls = parseBool(value);
                }
                else
                {
                    throw std::runtime_error(
                        "Unknown publisher ibkr key on line " +
                        std::to_string(lineNumber) + ": " + key);
                }
            }
            else if (currentSection == "ibkr.symbols")
            {
                config.m_ibkr.symbolsByConid[key] = value;
            }
            else if (currentSection == "export")
            {
                if (key == "enable_publisher_metrics_csv")
                {
                    config.m_export.enablePublisherMetricsCsv = parseBool(value);
                }
                else if (key == "publisher_metrics_csv_path")
                {
                    config.m_export.publisherMetricsCsvPath = value;
                }
                else if (key == "metrics_interval_ms")
                {
                    config.m_export.metricsIntervalMs =
                        static_cast<std::uint32_t>(std::stoul(value));
                }
                else
                {
                    throw std::runtime_error(
                        "Unknown publisher export key on line " +
                        std::to_string(lineNumber) + ": " + key);
                }
            }
            else
            {
                throw std::runtime_error(
                    "Unknown or missing publisher config section on line " +
                    std::to_string(lineNumber));
            }
        }

        return config;
    }
}
