#include "processing/RiskRuleEvaluator.h"

#include <cassert>

namespace
{
    mdp::MarketDataEvent makeEvent(double price, std::uint32_t volume)
    {
        mdp::MarketDataEvent event;
        event.symbol = "AAPL";
        event.price = price;
        event.volume = volume;
        event.exchangeTimestampNs = 1;
        event.ingestTimestampNs = 1;
        event.sequenceNumber = 1;
        return event;
    }
}

void runRiskRuleEvaluatorTests()
{
    mdp::RiskRuleEvaluator evaluator;

    const auto noPreviousStateAlerts = evaluator.evaluate(makeEvent(100.0, 100), std::nullopt);
    assert(noPreviousStateAlerts.empty());

    mdp::SymbolState previousState;
    previousState.lastPrice = 100.0;

    const auto normalAlerts = evaluator.evaluate(makeEvent(102.0, 100), previousState);
    assert(normalAlerts.empty());

    const auto priceJumpAlerts = evaluator.evaluate(makeEvent(106.0, 100), previousState);
    assert(priceJumpAlerts.size() == 1);
    assert(priceJumpAlerts[0].type == mdp::RuleType::PriceJump);

    const auto largeVolumeAlerts = evaluator.evaluate(makeEvent(102.0, 10000), previousState);
    assert(largeVolumeAlerts.size() == 1);
    assert(largeVolumeAlerts[0].type == mdp::RuleType::LargeVolume);

    const auto combinedAlerts = evaluator.evaluate(makeEvent(106.0, 10000), previousState);
    assert(combinedAlerts.size() == 2);
}
