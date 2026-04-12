#include "core/AppConfig.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>

namespace mdp::config
{
    namespace
    {
        AppConfig g_appConfig;

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

            throw std::runtime_error("Invalid boolean value: " + value);
        }

        std::size_t parseSize(const std::string& value)
        {
            return static_cast<std::size_t>(std::stoull(trim(value)));
        }

        std::uint32_t parseUint32(const std::string& value)
        {
            return static_cast<std::uint32_t>(std::stoul(trim(value)));
        }

        QueueFullPolicy parseQueueFullPolicy(const std::string& value)
        {
            const std::string normalized = toLower(trim(value));

            if (normalized == "blockproducer" || normalized == "block_producer")
            {
                return QueueFullPolicy::BlockProducer;
            }

            if (normalized == "dropincoming" || normalized == "drop_incoming")
            {
                return QueueFullPolicy::DropIncoming;
            }

            throw std::runtime_error("Invalid queue full policy: " + value);
        }

        QueueType parseQueueType(const std::string& value)
        {
            const std::string normalized = toLower(trim(value));

            if (normalized == "blockingbounded" || normalized == "blocking_bounded")
            {
                return QueueType::BlockingBounded;
            }

            if (normalized == "lockfreering" || normalized == "lock_free_ring")
            {
                return QueueType::LockFreeRing;
            }

            throw std::runtime_error("Invalid queue type: " + value);
        }
    }

    AppConfig AppConfig::loadFromIni(const std::filesystem::path& filePath)
    {
        std::ifstream input(filePath);
        if (!input.is_open())
        {
            throw std::runtime_error(
                "Failed to open config file: " + filePath.string());
        }

        AppConfig config;
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
                currentSection = toLower(trim(trimmedLine.substr(1, trimmedLine.size() - 2)));
                continue;
            }

            const std::size_t separator = trimmedLine.find('=');
            if (separator == std::string::npos)
            {
                throw std::runtime_error(
                    "Invalid config line " + std::to_string(lineNumber) +
                    ": expected key=value");
            }

            const std::string key = toLower(trim(trimmedLine.substr(0, separator)));
            const std::string value = trim(trimmedLine.substr(separator + 1));

            if (currentSection == "logging")
            {
                if (key == "enable_event_logging")
                {
                    config.m_logging.enableEventLogging = parseBool(value);
                }
                else if (key == "enable_alert_logging")
                {
                    config.m_logging.enableAlertLogging = parseBool(value);
                }
                else if (key == "enable_processing_stats_logging")
                {
                    config.m_logging.enableProcessingStatsLogging = parseBool(value);
                }
                else
                {
                    throw std::runtime_error(
                        "Unknown logging key on line " + std::to_string(lineNumber) +
                        ": " + key);
                }
            }
            else if (currentSection == "runtime")
            {
                if (key == "enable_load_test_mode")
                {
                    config.m_runtime.enableLoadTestMode = parseBool(value);
                }
                else if (key == "app_runtime_seconds")
                {
                    config.m_runtime.appRuntimeSeconds = parseSize(value);
                }
                else if (key == "num_workers")
                {
                    config.m_runtime.numWorkers = parseSize(value);
                }
                else if (key == "periodic_summary_interval_ms")
                {
                    config.m_runtime.periodicSummaryIntervalMs = parseSize(value);
                }
                else
                {
                    throw std::runtime_error(
                        "Unknown runtime key on line " + std::to_string(lineNumber) +
                        ": " + key);
                }
            }
            else if (currentSection == "producer")
            {
                if (key == "producer_count")
                {
                    config.m_producer.producerCount = parseSize(value);
                }
                else if (key == "producer_burst_size")
                {
                    config.m_producer.producerBurstSize = parseSize(value);
                }
                else if (key == "producer_sleep_us")
                {
                    config.m_producer.producerSleepUs = parseUint32(value);
                }
                else
                {
                    throw std::runtime_error(
                        "Unknown producer key on line " + std::to_string(lineNumber) +
                        ": " + key);
                }
            }
            else if (currentSection == "worker")
            {
                if (key == "processing_delay_us")
                {
                    config.m_worker.processingDelayUs = parseUint32(value);
                }
                else if (key == "optional_busy_work_iterations")
                {
                    config.m_worker.optionalBusyWorkIterations = parseSize(value);
                }
                else if (key == "queue_capacity")
                {
                    config.m_worker.workerQueueCapacity = parseSize(value);
                }
                else if (key == "queue_full_policy")
                {
                    config.m_worker.workerQueueFullStrategy = parseQueueFullPolicy(value);
                }
                else if (key == "queue_type")
                {
                    config.m_worker.workerQueueType = parseQueueType(value);
                }
                else
                {
                    throw std::runtime_error(
                        "Unknown worker key on line " + std::to_string(lineNumber) +
                        ": " + key);
                }
            }
            else if (currentSection == "reporting")
            {
                if (key == "processing_stats_log_interval")
                {
                    config.m_reporting.processingStatsLogInterval = parseSize(value);
                }
                else if (key == "print_processing_stats_summary")
                {
                    config.m_reporting.printProcessingStatsSummary = parseBool(value);
                }
                else if (key == "print_queue_metrics_summary")
                {
                    config.m_reporting.printQueueMetricsSummary = parseBool(value);
                }
                else if (key == "print_symbol_stats_summary")
                {
                    config.m_reporting.printSymbolStatsSummary = parseBool(value);
                }
                else
                {
                    throw std::runtime_error(
                        "Unknown reporting key on line " + std::to_string(lineNumber) +
                        ": " + key);
                }
            }
            else if (currentSection == "load_test")
            {
                if (key == "producer_burst_size")
                {
                    config.m_loadTest.producerBurstSize = parseSize(value);
                }
                else if (key == "producer_sleep_us")
                {
                    config.m_loadTest.producerSleepUs = parseUint32(value);
                }
                else if (key == "processing_delay_us")
                {
                    config.m_loadTest.processingDelayUs = parseUint32(value);
                }
                else if (key == "optional_busy_work_iterations")
                {
                    config.m_loadTest.optionalBusyWorkIterations = parseSize(value);
                }
                else if (key == "queue_capacity")
                {
                    config.m_loadTest.workerQueueCapacity = parseSize(value);
                }
                else if (key == "queue_type")
                {
                    config.m_loadTest.workerQueueType = parseQueueType(value);
                }
                else
                {
                    throw std::runtime_error(
                        "Unknown reporting key on line " + std::to_string(lineNumber) +
                        ": " + key);
                }
            }
            else
            {
                throw std::runtime_error(
                    "Unknown or missing section on line " + std::to_string(lineNumber));
            }
        }

        config.applyLoadTestOverrides();
        return config;
    }

    void AppConfig::applyLoadTestOverrides()
    {
        if (!m_runtime.enableLoadTestMode)
        {
            return;
        }

        m_producer.producerBurstSize = m_loadTest.producerBurstSize;
        m_producer.producerSleepUs = m_loadTest.producerSleepUs;
        m_worker.processingDelayUs = m_loadTest.processingDelayUs;
        m_worker.optionalBusyWorkIterations = m_loadTest.optionalBusyWorkIterations;
        m_worker.workerQueueCapacity = m_loadTest.workerQueueCapacity;
        m_worker.workerQueueType = m_loadTest.workerQueueType;
    }

    const AppConfig& get()
    {
        return g_appConfig;
    }

    void loadFromFile(const std::filesystem::path& filePath)
    {
        g_appConfig = AppConfig::loadFromIni(filePath);
    }
}
