#include "processing/SequenceTracker.h"

namespace mdp
{
    SequenceStatus SequenceTracker::evaluate(const Symbol& symbol, SequenceNumber sequenceNumber)
    {
        const auto it = m_lastSequenceBySymbol.find(symbol);
        if (it == m_lastSequenceBySymbol.end())
        {
            m_lastSequenceBySymbol.emplace(symbol, sequenceNumber);
            return SequenceStatus::New;
        }

        if (sequenceNumber == it->second)
        {
            return SequenceStatus::Duplicate;
        }

        if (sequenceNumber < it->second)
        {
            return SequenceStatus::OutOfOrder;
        }

        it->second = sequenceNumber;
        return SequenceStatus::New;
    }

    std::size_t SequenceTracker::getTrackedSymbolCount() const
    {
        return m_lastSequenceBySymbol.size();
    }
}
