#pragma once

#include <cstddef>
#include <cstdint>

namespace mdp::processing
{
    struct SymbolStatistics
    {
        std::size_t eventCount = 0;
        std::uint64_t totalVolume = 0;
        double minPrice = 0.0;
        double maxPrice = 0.0;
        double lastPrice = 0.0;
        double averagePrice = 0.0;
    };
}
