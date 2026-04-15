#pragma once

#include "api/domain/MarketDataEvent.h"

namespace mdp::processing::validation
{
    enum class ValidationError
    {
        None,
        EmptySymbol,
        InvalidPrice,
        InvalidVolume,
        InvalidExchangeTimestamp,
        InvalidSequenceNumber
    };

    struct ValidationResult
    {
        bool isValid{ true };
        ValidationError error{ ValidationError::None };
    };

    ValidationResult validate(const MarketDataEvent& event);
}
