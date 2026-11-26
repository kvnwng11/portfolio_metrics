#include <gtest/gtest.h>

#include "metrics/max_drawdown.h"
#include "structs/time_series.h"
#include "test_helpers.h"

using test_helpers::make_returns_context;
using test_helpers::make_series;

TEST(MaxDrawdownTest, MixedReturnsMatchesHandComputedValue)
{
    // returns = {0.10, -0.20, 0.05, -0.10, 0.30}
    // index path (starting from 1.0):
    //   1.00 -> 1.10 (new peak, drawdown 0)
    //   1.10 -> 0.88   (0.88-1.10)/1.10 = -0.181818...
    //   0.88 -> 0.924  (0.924-1.10)/1.10 = -0.16
    //   0.924 -> 0.8316 (0.8316-1.10)/1.10 = -0.244   <- worst point
    //   0.8316 -> 1.08108 (1.08108-1.10)/1.10 ≈ -0.01718 (still below old peak)
    // max drawdown = -0.244 (the deepest trough, even though later returns partially recover)
    TimeSeries series = make_series({0.10, -0.20, 0.05, -0.10, 0.30});
    MaxDrawdown metric;

    EXPECT_NEAR(metric.compute(make_returns_context(series)), -0.244, 1e-9);
}

TEST(MaxDrawdownTest, AlwaysPositiveReturnsProduceZeroDrawdown)
{
    // Every day sets a new peak, so drawdown never goes negative.
    TimeSeries series = make_series({0.01, 0.02, 0.015});
    MaxDrawdown metric;

    EXPECT_NEAR(metric.compute(make_returns_context(series)), 0.0, 1e-12);
}

TEST(MaxDrawdownTest, SinglePointCrashMatchesHandComputedValue)
{
    // A single -50% day: index goes 1.0 -> 0.5, peak stays 1.0 (set before the
    // decline), drawdown = (0.5 - 1.0) / 1.0 = -0.5.
    // Note: unlike the stdev-based metrics, MaxDrawdown's formula has no n-1
    // term, so a single data point is a valid (if minimal) input, not a
    // degenerate one — this is intentionally allowed, not an oversight.
    TimeSeries series = make_series({-0.50});
    MaxDrawdown metric;

    EXPECT_NEAR(metric.compute(make_returns_context(series)), -0.5, 1e-12);
}

TEST(MaxDrawdownTest, RecoveryDoesNotErasePreviousWorstDrawdown)
{
    // A decline followed by a full recovery to a new peak. The max drawdown
    // should still reflect the worst point reached along the way, not just
    // the final (fully recovered, zero-drawdown) state.
    //   1.00 -> 0.70  (decline of -0.30, peak stays 1.00)
    //   0.70 -> 1.05  (new peak, drawdown back to 0.0)
    // max drawdown should remain -0.30 despite ending at a new high.
    TimeSeries series = make_series({-0.30, 0.5});
    MaxDrawdown metric;

    EXPECT_NEAR(metric.compute(make_returns_context(series)), -0.30, 1e-9);
}

TEST(MaxDrawdownTest, ThrowsOnEmptySeries)
{
    TimeSeries series; // no points added
    MaxDrawdown metric;

    EXPECT_THROW(metric.compute(make_returns_context(series)), std::runtime_error);
}