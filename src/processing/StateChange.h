#pragma once

#include "processing/SymbolStateStore.h"

#include <optional>

namespace mdp
{
    struct StateChange
    {
        Symbol symbol;
        std::optional<SymbolState> previousState;
        SymbolState currentState;
    };
}
