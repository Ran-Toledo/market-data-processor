#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace mdp::publisher
{
    struct IbkrQuote
    {
        std::string conid;
        std::optional<double> lastPrice;
        std::optional<double> bidPrice;
        std::optional<double> askPrice;
        std::optional<std::uint32_t> bidSize;
        std::optional<std::uint32_t> askSize;
        std::optional<std::uint64_t> updatedMs;
    };
}
