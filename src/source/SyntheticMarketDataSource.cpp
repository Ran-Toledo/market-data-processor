#include "source/SyntheticMarketDataSource.h"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <utility>

namespace mdp
{
    SyntheticMarketDataSource::SyntheticMarketDataSource(
        std::vector<Symbol> symbols,
        std::size_t maxMessages,
        std::uint32_t seed)
        : m_symbols(std::move(symbols))
        , m_maxMessages(maxMessages)
        , m_rng(seed)
        , m_volumeDistribution(100U, 1000U)
        , m_priceDeltaDistribution(-1.0, 1.0)
    {
        if (m_symbols.empty())
        {
            throw std::invalid_argument("SyntheticMarketDataSource requires at least one symbol");
        }

        for (const Symbol& symbol : m_symbols)
        {
            m_lastPrices.emplace(symbol, 100.0);
        }
    }

    bool SyntheticMarketDataSource::next(MarketDataEvent& outEvent)
    {
        if (m_generatedMessages >= m_maxMessages)
        {
            return false;
        }

        const Symbol symbol = nextSymbol();
        const double price = nextPrice(symbol);
        const std::uint32_t volume = nextVolume();
        const TimestampNs exchangeTimestampNs = nextExchangeTimestamp();

        outEvent.symbol = symbol;
        outEvent.price = price;
        outEvent.volume = volume;
        outEvent.exchangeTimestampNs = exchangeTimestampNs;
        outEvent.ingestTimestampNs = 0;
        outEvent.sequenceNumber = m_nextSequenceNumber;

        ++m_generatedMessages;
        ++m_nextSequenceNumber;

        return true;
    }

    Symbol SyntheticMarketDataSource::nextSymbol()
    {
        const Symbol symbol = m_symbols[m_symbolIndex];
        m_symbolIndex = (m_symbolIndex + 1) % m_symbols.size();
        return symbol;
    }

    double SyntheticMarketDataSource::nextPrice(const Symbol& symbol)
    {
        double& lastPrice = m_lastPrices.at(symbol);
        lastPrice = std::max(1.0, lastPrice + m_priceDeltaDistribution(m_rng));
        return lastPrice;
    }

    std::uint32_t SyntheticMarketDataSource::nextVolume()
    {
        return m_volumeDistribution(m_rng);
    }

    TimestampNs SyntheticMarketDataSource::nextExchangeTimestamp() const
    {
        const auto now = std::chrono::steady_clock::now().time_since_epoch();
        return static_cast<TimestampNs>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
    }
}
