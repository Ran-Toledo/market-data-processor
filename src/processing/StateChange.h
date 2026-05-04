#pragma once

#include "api/domain/Types.h"
#include "processing/SymbolState.h"

#include <optional>

namespace mdp::processing
{
    struct StateChange
    {
        Symbol symbol;
        std::optional<SymbolState> previousState;
        SymbolState currentState;
    };
}
