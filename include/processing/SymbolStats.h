// SymbolStats.h
#pragma once

#include <cstdint>

namespace mdp
{
    struct SymbolStats
    {
        std::uint64_t messageCount{ 0 };
        std::uint64_t totalVolume{ 0 };
        double latestPrice{ 0.0 };
    };
}
