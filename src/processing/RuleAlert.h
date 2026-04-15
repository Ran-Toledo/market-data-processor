#pragma once

#include "api/domain/Types.h"

#include <string>

namespace mdp::processing
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
