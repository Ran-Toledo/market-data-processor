#include "pipeline/BlockingBoundedEventQueue.h"
#include "pipeline/LockFreeRingEventQueue.h"

#include <cassert>

namespace
{
    mdp::MarketDataEvent makeEvent(mdp::SequenceNumber sequenceNumber)
    {
        mdp::MarketDataEvent event;
        event.symbol = "AAPL";
        event.price = 100.0;
        event.volume = 10;
        event.exchangeTimestampNs = 1;
        event.ingestTimestampNs = 1;
        event.sequenceNumber = sequenceNumber;
        return event;
    }

    void assertDropIncomingBehavior(mdp::IEventQueue& queue)
    {
        assert(queue.push(makeEvent(1)));
        assert(queue.push(makeEvent(2)));
        assert(!queue.push(makeEvent(3)));

        const auto fullMetrics = queue.getMetricsSnapshot();
        assert(fullMetrics.currentDepth == 2);
        assert(fullMetrics.maxDepth == 2);
        assert(fullMetrics.droppedCount == 1);
        assert(fullMetrics.failedEnqueueCount == 1);

        mdp::MarketDataEvent event;
        assert(queue.pop(event));
        assert(event.sequenceNumber == 1);
        assert(queue.pop(event));
        assert(event.sequenceNumber == 2);

        queue.close();
        assert(!queue.pop(event));
    }
}

void runEventQueueTests()
{
    {
        mdp::BlockingBoundedEventQueue queue(
            2,
            mdp::config::QueueFullPolicy::DropIncoming);
        assertDropIncomingBehavior(queue);
    }

    {
        mdp::LockFreeRingEventQueue queue(
            2,
            mdp::config::QueueFullPolicy::DropIncoming);
        assertDropIncomingBehavior(queue);
    }

    {
        mdp::LockFreeRingEventQueue queue(
            2,
            mdp::config::QueueFullPolicy::DropIncoming);
        assert(queue.push(makeEvent(1)));
        queue.closeAndDiscard();

        mdp::MarketDataEvent event;
        assert(!queue.pop(event));
        assert(queue.getMetricsSnapshot().currentDepth == 0);
    }
}
