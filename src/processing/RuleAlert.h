#pragma once

#include "core/Types.h"

#include <string>

namespace mdp
{
    enum class RuleType
    {
        PriceJump,
        LargeVolume
    };

    struct RuleAlert
    {
        RuleType type;
        Symbol symbol;
        std::string message;
    };
}
