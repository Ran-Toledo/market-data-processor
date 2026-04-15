// Clock.h
#pragma once

#include "api/domain/Types.h"

#include <chrono>

namespace mdp::clock
{
    inline TimestampNs nowNs()
    {
        return static_cast<TimestampNs>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
    }
}
