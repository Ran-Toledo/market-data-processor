#pragma once

#include "output/IEventSink.h"

#include <filesystem>
#include <fstream>
#include <mutex>

namespace mdp::output
{
    class CsvEventSink : public IEventSink
    {
    public:
        explicit CsvEventSink(const std::filesystem::path& processedEventsPath);

        void publishProcessedEvent(const MarketDataEvent& event) override;
        void publishAlert(const processing::RuleAlert& alert) override;
        void publishStateChange(const processing::StateChange& stateChange) override;

    private:
        std::mutex m_mutex;
        std::ofstream m_processedEvents;
    };
}
