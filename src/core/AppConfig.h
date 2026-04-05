// AppConfig.h
#pragma once

#include <cstddef>
#include <cstdint>

namespace mdp::config
{
    inline bool enableEventLogging = true;
    inline bool enableAlertLogging = true;
    inline bool enableProcessingStatsLogging = true;
    inline std::size_t processingStatsLogInterval = 1000;
    inline std::uint32_t sourceSleepMs = 1;
    inline std::size_t appRuntimeMs = 10;
    inline std::size_t numOfWorkers = 1;
}
