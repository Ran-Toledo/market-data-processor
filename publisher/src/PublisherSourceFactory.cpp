#include "PublisherSourceFactory.h"

#include "synthetic/SyntheticPublisherSource.h"
#include "ibkr/IbkrMarketDataClient.h"
#include "ibkr/IbkrMarketDataSource.h"

#include <stdexcept>

namespace mdp::publisher
{
    std::unique_ptr<IPublisherSource> createPublisherSource(
        const config::PublisherConfig& config)
    {
        if (config.source().sourceType == "synthetic")
        {
            return std::make_unique<SyntheticPublisherSource>(
                config.source().symbolCount,
                config.source().symbolOffset);
        }

        if (config.source().sourceType == "ibkr")
        {
            return std::make_unique<IbkrMarketDataSource>(
                config.ibkr(),
                std::make_unique<IbkrMarketDataClient>(
                    config.ibkr(),
                    std::make_unique<WinHttpIbkrHttpClient>()));
        }

        throw std::runtime_error(
            "Unsupported publisher source type: " + config.source().sourceType);
    }
}
