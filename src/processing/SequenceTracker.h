#pragma once

#include "core/Types.h"

#include <mutex>
#include <unordered_map>

namespace mdp
{
    enum class SequenceStatus
    {
        New,
        Duplicate,
        OutOfOrder
    };

    class SequenceTracker
    {
    public:
        SequenceStatus evaluate(const Symbol& symbol, SequenceNumber sequenceNumber);

        std::size_t getTrackedSymbolCount() const;

    private:
        mutable std::mutex m_mutex;
        std::unordered_map<Symbol, SequenceNumber> m_lastSequenceBySymbol;
    };
}
