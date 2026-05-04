#pragma once

#include "IPublisherSource.h"

#include <random>
#include <vector>

namespace mdp::publisher
{
    class SyntheticPublisherSource final : public IPublisherSource
    {
    public:
        struct SymbolProfile
        {
            std::string symbol;
            double basePrice{ 0.0 };
            double maxDeviation{ 0.0 };
        };

    public:
        SyntheticPublisherSource();
        explicit SyntheticPublisherSource(std::size_t symbolCount);
        SyntheticPublisherSource(std::size_t symbolCount, std::size_t symbolOffset);

        bool next(MarketDataEvent& outEvent) override;

    private:
        struct SymbolRuntimeState
        {
            double lastPrice{ 0.0 };
            SequenceNumber nextSequenceNumber{ 0 };
        };

        MarketDataEvent generateEvent();

        std::vector<SymbolProfile> m_symbolProfiles;
        std::vector<SymbolRuntimeState> m_symbolStates;
        std::mt19937 m_generator;
    };
}
