#include <iostream>
#include <string>

void runSequenceTrackerTests();
void runEventProcessorTests();
void runEventQueueTests();
void runLatencyRecorderTests();
void runMetricsCollectorTests();
void runRiskRuleEvaluatorTests();
void runSymbolStatsTests();
void runSymbolStateStoreTests();
void runPipelineIntegrationTests();

namespace
{
    bool runSuite(const std::string& suiteName)
    {
        if (suiteName == "unit.sequence_tracker")
        {
            runSequenceTrackerTests();
            return true;
        }

        if (suiteName == "unit.event_processor")
        {
            runEventProcessorTests();
            return true;
        }

        if (suiteName == "unit.event_queue")
        {
            runEventQueueTests();
            return true;
        }

        if (suiteName == "unit.latency_recorder")
        {
            runLatencyRecorderTests();
            return true;
        }

        if (suiteName == "unit.metrics_collector")
        {
            runMetricsCollectorTests();
            return true;
        }

        if (suiteName == "unit.risk_rule_evaluator")
        {
            runRiskRuleEvaluatorTests();
            return true;
        }

        if (suiteName == "unit.symbol_stats")
        {
            runSymbolStatsTests();
            return true;
        }

        if (suiteName == "unit.symbol_state_store")
        {
            runSymbolStateStoreTests();
            return true;
        }

        if (suiteName == "integration.pipeline")
        {
            runPipelineIntegrationTests();
            return true;
        }

        return false;
    }
}

int main(int argc, char** argv)
{
    if (argc > 1)
    {
        const std::string suiteName = argv[1];
        if (!runSuite(suiteName))
        {
            std::cerr << "Unknown test suite: " << suiteName << '\n';
            return 1;
        }

        return 0;
    }

    const char* allSuites[] =
    {
        "unit.sequence_tracker",
        "unit.event_processor",
        "unit.event_queue",
        "unit.latency_recorder",
        "unit.metrics_collector",
        "unit.risk_rule_evaluator",
        "unit.symbol_stats",
        "unit.symbol_state_store",
        "integration.pipeline"
    };

    for (const char* suiteName : allSuites)
    {
        runSuite(suiteName);
    }

    return 0;
}
