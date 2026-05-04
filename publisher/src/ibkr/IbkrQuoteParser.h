#pragma once

#include "ibkr/IbkrQuote.h"

#include <optional>
#include <string>
#include <vector>

namespace mdp::publisher
{
    std::vector<IbkrQuote> parseIbkrSnapshotPayload(const std::string& payload);
    std::optional<IbkrQuote> parseIbkrStreamingPayload(const std::string& payload);
    void mergeIbkrQuote(IbkrQuote& target, const IbkrQuote& update);
}
