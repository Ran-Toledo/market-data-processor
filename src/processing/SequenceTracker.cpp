#include "processing/SequenceTracker.h"

namespace mdp
{
    SequenceResult SequenceTracker::evaluate(const Symbol& symbol, SequenceNumber sequenceNumber)
    {
        const auto it = m_lastSequenceBySymbol.find(symbol);
        if (it == m_lastSequenceBySymbol.end())
        {
            m_lastSequenceBySymbol.emplace(symbol, sequenceNumber);
            return {
                SequenceStatus::New,
                0,
                sequenceNumber,
                0
            };
        }

        const SequenceNumber previousSequenceNumber = it->second;
        const SequenceNumber expectedSequenceNumber = previousSequenceNumber + 1;

        if (sequenceNumber == it->second)
        {
            return {
                SequenceStatus::Duplicate,
                previousSequenceNumber,
                sequenceNumber,
                expectedSequenceNumber
            };
        }

        if (sequenceNumber < it->second)
        {
            return {
                SequenceStatus::OutOfOrder,
                previousSequenceNumber,
                sequenceNumber,
                expectedSequenceNumber
            };
        }

        it->second = sequenceNumber;
        return {
            sequenceNumber == expectedSequenceNumber ? SequenceStatus::New : SequenceStatus::Gap,
            previousSequenceNumber,
            sequenceNumber,
            expectedSequenceNumber
        };
    }

    std::size_t SequenceTracker::getTrackedSymbolCount() const
    {
        return m_lastSequenceBySymbol.size();
    }
}
