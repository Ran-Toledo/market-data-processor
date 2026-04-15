#pragma once

#include "IPublisherSource.h"

#include <random>
#include <vector>

namespace mdp::publisher
{
    class SyntheticPublisherSource final : public IPublisherSource
    {
    public:
        SyntheticPublisherSource();

        bool next(MarketDataEvent& outEvent) override;

    private:
        struct SymbolRuntimeState
        {
            double lastPrice{ 0.0 };
            SequenceNumber nextSequenceNumber{ 0 };
        };

        MarketDataEvent generateEvent();

        std::vector<SymbolRuntimeState> m_symbolStates;
        std::mt19937 m_generator;
    };
}
