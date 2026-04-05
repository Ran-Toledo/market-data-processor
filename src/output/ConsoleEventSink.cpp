#include "output/ConsoleEventSink.h"

#include <iostream>

namespace mdp
{
    void ConsoleEventSink::publishProcessedEvent(const MarketDataEvent& event)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::cout << "[PROCESSED] " << event << '\n';
    }

    void ConsoleEventSink::publishAlert(const RuleAlert& alert)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::cout << "[ALERT] Symbol=" << alert.symbol
            << " Message=" << alert.message << '\n';
    }
}
