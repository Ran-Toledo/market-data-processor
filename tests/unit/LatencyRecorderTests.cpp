#include "metrics/LatencyRecorder.h"

#include <cassert>

void runLatencyRecorderTests()
{
    mdp::metrics::LatencyRecorder recorder;

    assert(recorder.getCount() == 0);
    assert(recorder.getAverageLatencyNs() == 0);
    assert(recorder.getMinLatencyNs() == 0);
    assert(recorder.getMaxLatencyNs() == 0);

    recorder.record(100);
    recorder.record(50);
    recorder.record(250);

    assert(recorder.getCount() == 3);
    assert(recorder.getAverageLatencyNs() == 133);
    assert(recorder.getMinLatencyNs() == 50);
    assert(recorder.getMaxLatencyNs() == 250);
    assert(recorder.getPercentileLatencyNs(50.0) == 128);
    assert(recorder.getPercentileLatencyNs(95.0) == 256);
    assert(recorder.getPercentileLatencyNs(99.0) == 256);
}
