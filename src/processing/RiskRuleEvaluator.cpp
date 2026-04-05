#include "processing/RiskRuleEvaluator.h"

#include <cmath>

namespace mdp
{
    std::vector<RuleAlert> RiskRuleEvaluator::evaluate(
        const MarketDataEvent& event,
        const std::optional<SymbolState>& previousState) const
    {
        std::vector<RuleAlert> alerts;

        if (previousState.has_value() && previousState->lastPrice > 0.0)
        {
            const double relativeMove =
                std::abs((event.price - previousState->lastPrice) / previousState->lastPrice);

            if (relativeMove > s_priceJumpThreshold)
            {
                alerts.push_back({
                    RuleType::PriceJump,
                    event.symbol,
                    "Price jump exceeded threshold"
                    });
            }
        }

        if (event.volume > s_largeVolumeThreshold)
        {
            alerts.push_back({
                RuleType::LargeVolume,
                event.symbol,
                "Volume exceeded threshold"
                });
        }

        return alerts;
    }
}
