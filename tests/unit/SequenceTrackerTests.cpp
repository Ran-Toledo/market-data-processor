#include "processing/SequenceTracker.h"

#include <cassert>

void runSequenceTrackerTests()
{
    mdp::processing::SequenceTracker tracker;

    const auto first = tracker.evaluate("AAPL", 1);
    assert(first.status == mdp::processing::SequenceStatus::New);
    assert(first.shouldProcess());

    const auto duplicate = tracker.evaluate("AAPL", 1);
    assert(duplicate.status == mdp::processing::SequenceStatus::Duplicate);
    assert(!duplicate.shouldProcess());
    assert(duplicate.previousSequenceNumber == 1);
    assert(duplicate.expectedSequenceNumber == 2);

    const auto gap = tracker.evaluate("AAPL", 3);
    assert(gap.status == mdp::processing::SequenceStatus::Gap);
    assert(gap.shouldProcess());
    assert(gap.previousSequenceNumber == 1);
    assert(gap.expectedSequenceNumber == 2);

    const auto outOfOrder = tracker.evaluate("AAPL", 2);
    assert(outOfOrder.status == mdp::processing::SequenceStatus::OutOfOrder);
    assert(!outOfOrder.shouldProcess());

    const auto contiguous = tracker.evaluate("AAPL", 4);
    assert(contiguous.status == mdp::processing::SequenceStatus::New);
    assert(contiguous.shouldProcess());
}
