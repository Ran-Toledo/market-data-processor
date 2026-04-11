#pragma once

#include "core/Types.h"

#include <unordered_map>

namespace mdp
{
    enum class SequenceStatus
    {
        New,
        Duplicate,
        OutOfOrder,
        Gap
    };

    struct SequenceResult
    {
        SequenceStatus status{ SequenceStatus::New };
        SequenceNumber previousSequenceNumber{ 0 };
        SequenceNumber incomingSequenceNumber{ 0 };
        SequenceNumber expectedSequenceNumber{ 0 };

        bool shouldProcess() const
        {
            return status == SequenceStatus::New || status == SequenceStatus::Gap;
        }
    };

    class SequenceTracker
    {
    public:
        SequenceResult evaluate(const Symbol& symbol, SequenceNumber sequenceNumber);

        std::size_t getTrackedSymbolCount() const;

    private:
        std::unordered_map<Symbol, SequenceNumber> m_lastSequenceBySymbol;
    };
}
