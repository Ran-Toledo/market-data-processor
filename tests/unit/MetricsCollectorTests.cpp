#include "metrics/MetricsCollector.h"

#include <cassert>

void runMetricsCollectorTests()
{
    mdp::metrics::MetricsCollector metrics;

    assert(metrics.getProcessed() == 0);
    assert(metrics.getValid() == 0);
    assert(metrics.getInvalid() == 0);
    assert(metrics.getDuplicate() == 0);
    assert(metrics.getOutOfOrder() == 0);
    assert(metrics.getSequenceGap() == 0);
    assert(metrics.getAlerts() == 0);

    metrics.onProcessed();
    metrics.onValid();
    metrics.onInvalid();
    metrics.onDuplicate();
    metrics.onOutOfOrder();
    metrics.onSequenceGap();
    metrics.onAlerts(3);

    assert(metrics.getProcessed() == 1);
    assert(metrics.getValid() == 1);
    assert(metrics.getInvalid() == 1);
    assert(metrics.getDuplicate() == 1);
    assert(metrics.getOutOfOrder() == 1);
    assert(metrics.getSequenceGap() == 1);
    assert(metrics.getAlerts() == 3);
}
