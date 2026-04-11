#include "processing/SymbolStateStore.h"

namespace mdp
{
    void SymbolStateStore::update(const MarketDataEvent& event)
    {
        SymbolState& state = m_symbolStates[event.symbol];
        state.lastPrice = event.price;
        state.lastVolume = event.volume;
        state.lastExchangeTimestampNs = event.exchangeTimestampNs;
        state.lastIngestTimestampNs = event.ingestTimestampNs;
        state.lastSequenceNumber = event.sequenceNumber;
    }

    std::optional<SymbolState> SymbolStateStore::tryGet(const Symbol& symbol) const
    {
        const auto it = m_symbolStates.find(symbol);
        if (it == m_symbolStates.end())
        {
            return std::nullopt;
        }

        return it->second;
    }

    std::unordered_map<Symbol, SymbolState> SymbolStateStore::snapshot() const
    {
        return m_symbolStates;
    }

    std::size_t SymbolStateStore::getTrackedSymbolCount() const
    {
        return m_symbolStates.size();
    }
}
