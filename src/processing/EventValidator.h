#pragma once

#include "core/MarketDataEvent.h"

namespace mdp::validation
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
