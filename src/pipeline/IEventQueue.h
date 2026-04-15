#pragma once

#include "api/domain/MarketDataEvent.h"
#include "pipeline/QueueMetricsSnapshot.h"

#include <cstddef>

namespace mdp::pipeline
{
    class IEventQueue
    {
    public:
        virtual ~IEventQueue() = default;

        virtual bool push(const MarketDataEvent& event) = 0;
        virtual bool pop(MarketDataEvent& event) = 0;
        virtual void close() = 0;
        virtual void closeAndDiscard() = 0;
        virtual std::size_t capacity() const = 0;
        virtual QueueMetricsSnapshot getMetricsSnapshot() const = 0;
    };
}
