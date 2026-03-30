// SymbolStateStore.h
#pragma once

#include "core/MarketDataEvent.h"
#include "processing/SymbolStats.h"

#include <mutex>
#include <optional>
#include <unordered_map>

namespace mdp
{
    class SymbolStateStore
    {
    public:
        SymbolStateStore() = default;

        void update(const MarketDataEvent& event);

        std::optional<SymbolStats> get(const Symbol& symbol) const;
        std::unordered_map<Symbol, SymbolStats> snapshot() const;

    private:
        mutable std::mutex m_mutex;
        std::unordered_map<Symbol, SymbolStats> m_statsBySymbol;
    };
}
