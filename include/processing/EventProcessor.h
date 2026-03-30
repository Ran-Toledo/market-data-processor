// EventProcessor.h
#pragma once

#include "core/MarketDataEvent.h"
#include "processing/SymbolStateStore.h"

namespace mdp
{
    class EventProcessor
    {
    public:
        explicit EventProcessor(SymbolStateStore& stateStore);

        void process(const MarketDataEvent& event);

    private:
        SymbolStateStore& m_stateStore;
    };
}
