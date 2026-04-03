// SymbolStateStore.h
#pragma once

#include "core/MarketDataEvent.h"

#include <mutex>
#include <optional>
#include <unordered_map>

namespace mdp
{
    struct SymbolState
    {
        double lastPrice = 0.0;
        std::uint32_t lastVolume = 0;
        TimestampNs lastExchangeTimestampNs = 0;
        TimestampNs lastIngestTimestampNs = 0;
        SequenceNumber lastSequenceNumber = 0;
    };

    class SymbolStateStore
    {
    public:
        void update(const MarketDataEvent& event);

        std::optional<SymbolState> tryGet(const Symbol& symbol) const;

        std::size_t getTrackedSymbolCount() const;

    private:
        mutable std::mutex m_mutex;
        std::unordered_map<Symbol, SymbolState> m_symbolStates;
    };
}
