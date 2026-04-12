#pragma once

#include <cstddef>
#include <cstdint>

namespace mdp
{
    struct QueueMetricsSnapshot
    {
        std::size_t currentDepth{ 0 };
        std::size_t maxDepth{ 0 };
        std::uint64_t droppedCount{ 0 };
        std::uint64_t failedEnqueueCount{ 0 };
    };
}
