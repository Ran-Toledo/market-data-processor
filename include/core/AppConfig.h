// AppConfig.h
#pragma once

#include <cstddef>
#include <cstdint>

namespace mdp::config
{
    inline bool enableEventLogging = true;
    inline bool enableProcessingStatsLogging = true;
    inline std::size_t processingStatsLogInterval = 1000;
    inline std::uint32_t sourceSleepMs = 1;
}
