#include "processing/SequenceTracker.h"

namespace mdp
{
    SequenceStatus SequenceTracker::evaluate(const Symbol& symbol, SequenceNumber sequenceNumber)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

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
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_lastSequenceBySymbol.size();
    }
}
