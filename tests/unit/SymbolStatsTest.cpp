#include gtestgtest.h

#include processingSymbolStats.h
#include coreMarketDataEvent.h

namespace mdptest
{
    TEST(SymbolStatsTests, TracksBasicStatistics)
    {
        SymbolStats stats;

        MarketDataEvent e1{ AAPL, 100.0, 10, 0, 0, 1 };
        MarketDataEvent e2{ AAPL, 110.0, 20, 0, 0, 2 };
        MarketDataEvent e3{ AAPL, 90.0, 30, 0, 0, 3 };

        stats.record(e1);
        stats.record(e2);
        stats.record(e3);

        const auto snapshot = stats.getSnapshot(AAPL);

        ASSERT_TRUE(snapshot.has_value());
        EXPECT_EQ(snapshot-count, 3);
        EXPECT_DOUBLE_EQ(snapshot-minPrice, 90.0);
        EXPECT_DOUBLE_EQ(snapshot-maxPrice, 110.0);
        EXPECT_DOUBLE_EQ(snapshot-averagePrice, 100.0);
    }
}
