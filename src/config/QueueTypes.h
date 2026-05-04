#pragma once

namespace mdp::config
{
    enum class QueueFullPolicy
    {
        BlockSubmitter,
        DropIncoming
    };

    enum class QueueType
    {
        BlockingBounded,
        LockFreeRing
    };
}
