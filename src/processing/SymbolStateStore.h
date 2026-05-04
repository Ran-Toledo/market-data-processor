#pragma once

#include "api/domain/Types.h"
#include "processing/SymbolState.h"

#include <optional>
#include <unordered_map>

namespace mdp
{
    struct MarketDataEvent;
}

namespace mdp::processing
{
    class SymbolStateStore
    {
    public:
        void update(const MarketDataEvent& event);

        std::optional<SymbolState> tryGet(const Symbol& symbol) const;

        std::unordered_map<Symbol, SymbolState> snapshot() const;

        std::size_t getTrackedSymbolCount() const;

    private:
        std::unordered_map<Symbol, SymbolState> m_symbolStates;
    };
}
