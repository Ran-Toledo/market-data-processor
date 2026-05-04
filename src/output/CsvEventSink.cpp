#include "output/CsvEventSink.h"

#include "api/domain/MarketDataEvent.h"

#include <ostream>
#include <stdexcept>
#include <string>

namespace mdp::output
{
    namespace
    {
        void ensureParentDirectory(const std::filesystem::path& path)
        {
            const auto parent = path.parent_path();
            if (!parent.empty())
            {
                std::filesystem::create_directories(parent);
            }
        }

        void writeCsvString(std::ostream& output, const std::string& value)
        {
            output << '"';
            for (const char c : value)
            {
                if (c == '"')
                {
                    output << "\"\"";
                }
                else
                {
                    output << c;
                }
            }
            output << '"';
        }
    }

    CsvEventSink::CsvEventSink(const std::filesystem::path& processedEventsPath)
    {
        ensureParentDirectory(processedEventsPath);
        m_processedEvents.open(processedEventsPath, std::ios::out | std::ios::trunc);

        if (!m_processedEvents.is_open())
        {
            throw std::runtime_error(
                "Failed to open processed events CSV: " + processedEventsPath.string());
        }

        m_processedEvents
            << "symbol,price,volume,exchangeTimestampNs,ingestTimestampNs,"
            << "enqueueTimestampNs,sequenceNumber\n";
    }

    void CsvEventSink::publishProcessedEvent(const MarketDataEvent& event)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        writeCsvString(m_processedEvents, event.symbol);
        m_processedEvents
            << ','
            << event.price << ','
            << event.volume << ','
            << event.exchangeTimestampNs << ','
            << event.ingestTimestampNs << ','
            << event.enqueueTimestampNs << ','
            << event.sequenceNumber << '\n';
    }

    void CsvEventSink::publishAlert(const processing::RuleAlert&)
    {
    }

    void CsvEventSink::publishStateChange(const processing::StateChange&)
    {
    }
}
