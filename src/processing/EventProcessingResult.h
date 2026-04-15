#pragma once

#include "processing/EventValidator.h"
#include "processing/RuleAlert.h"
#include "processing/SequenceTracker.h"
#include "processing/StateChange.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace mdp::processing
{
    struct EventProcessingResult
    {
        validation::ValidationResult validation;
        SequenceResult sequence;
        std::vector<RuleAlert> alerts;
        std::optional<StateChange> stateChange;
        std::uint64_t latencyNs{ 0 };
        bool processed{ false };
    };
}
