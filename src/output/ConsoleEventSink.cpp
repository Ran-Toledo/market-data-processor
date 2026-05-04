#include "output/ConsoleEventSink.h"

#include "api/domain/MarketDataEvent.h"
#include "processing/RuleAlert.h"
#include "processing/StateChange.h"

#include <iostream>

namespace mdp::output
{
    void ConsoleEventSink::publishProcessedEvent(const MarketDataEvent& event)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::cout << "[PROCESSED] " << event << '\n';
    }

    void ConsoleEventSink::publishAlert(const processing::RuleAlert& alert)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::cout << "[ALERT] Symbol=" << alert.symbol
            << " Message=" << alert.message << '\n';
    }

    void ConsoleEventSink::publishStateChange(const processing::StateChange& stateChange)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::cout << "[STATE] Symbol=" << stateChange.symbol
            << " LastPrice=" << stateChange.currentState.lastPrice
            << " LastVolume=" << stateChange.currentState.lastVolume
            << " LastSequence=" << stateChange.currentState.lastSequenceNumber
            << '\n';
    }
}
