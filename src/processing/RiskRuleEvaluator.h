#pragma once

#include "core/MarketDataEvent.h"
#include "processing/RuleAlert.h"
#include "processing/SymbolStateStore.h"

#include <optional>
#include <vector>

namespace mdp
{
    class RiskRuleEvaluator
    {
    public:
        std::vector<RuleAlert> evaluate(
            const MarketDataEvent& event,
            const std::optional<SymbolState>& previousState) const;

    private:
        static constexpr double s_priceJumpThreshold = 0.05;
        static constexpr std::uint32_t s_largeVolumeThreshold = 9500;
    };
}
