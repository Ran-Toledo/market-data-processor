#include "PublisherConfig.h"

#include <algorithm>
#include <cctype>
#include <fstream>
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
            else if (currentSection == "source")
            {
                if (key == "source_type")
                {
                    config.m_source.sourceType = toLower(value);
                }
                else
                {
                    throw std::runtime_error(
                        "Unknown publisher source key on line " +
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
