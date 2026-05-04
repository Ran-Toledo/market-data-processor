#pragma once

#include "IPublisherSource.h"
#include "PublisherConfig.h"

#include <memory>

namespace mdp::publisher
{
    std::unique_ptr<IPublisherSource> createPublisherSource(
        const config::PublisherConfig& config);
}
