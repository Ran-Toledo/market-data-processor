#include <gtest/gtest.h>

#include "processing/EventProcessor.h"
#include "processing/SymbolStateStore.h"
#include "processing/SymbolStats.h"
#include "core/MarketDataEvent.h"

namespace mdp::test
{
    TEST(PipelineIntegrationTests, ProcessesEventsEndToEnd)
    {
        SymbolStateStore stateStore;
        SymbolStats stats;
        EventProcessor processor(stateStore, stats);

        MarketDataEvent e1{ "AAPL", 100.0, 10, 1000, 1000, 1 };
        MarketDataEvent e2{ "AAPL", 105.0, 20, 2000, 2000, 2 };

        processor.process(e1);
        processor.process(e2);

        const auto state = stateStore.get("AAPL");
        ASSERT_TRUE(state.has_value());

        EXPECT_DOUBLE_EQ(state->lastPrice, 105.0);
        EXPECT_EQ(state->lastVolume, 20u);
        EXPECT_EQ(state->lastSequenceNumber, 2u);

        const auto snapshot = stats.getSnapshot("AAPL");
        ASSERT_TRUE(snapshot.has_value());
        EXPECT_EQ(snapshot->count, 2);
    }
}
