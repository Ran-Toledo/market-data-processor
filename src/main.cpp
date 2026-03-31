#include "core/MarketDataEvent.h"
#include "source/SyntheticMarketDataSource.h"

#include <iostream>
#include <vector>

int main()
{
    std::vector<mdp::Symbol> symbols
    {
        "AAPL",
        "MSFT",
        "GOOG"
    };

    mdp::SyntheticMarketDataSource source(symbols, 10000);

    mdp::MarketDataEvent event{};

    while (source.next(event))
    {
        std::cout
            << "symbol=" << event.symbol
            << ", price=" << event.price
            << ", volume=" << event.volume
            << ", exchangeTs=" << event.exchangeTimestampNs
            << ", ingestTs=" << event.ingestTimestampNs
            << ", seq=" << event.sequenceNumber
            << '\n';
    }

    return 0;
}
