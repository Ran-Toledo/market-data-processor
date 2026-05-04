#include "processing/EventValidator.h"

#include "api/domain/MarketDataEvent.h"

namespace mdp::processing::validation
{
    ValidationResult validate(const MarketDataEvent& event)
    {
        if (event.symbol.empty())
        {
            return { false, ValidationError::EmptySymbol };
        }

        if (event.price <= 0.0)
        {
            return { false, ValidationError::InvalidPrice };
        }

        if (event.volume == 0)
        {
            return { false, ValidationError::InvalidVolume };
        }

        if (event.exchangeTimestampNs == 0)
        {
            return { false, ValidationError::InvalidExchangeTimestamp };
        }

        if (event.sequenceNumber == 0)
        {
            return { false, ValidationError::InvalidSequenceNumber };
        }

        return { true, ValidationError::None };
    }
}
