#include <gtest/gtest.h>

#include "metrics/value_at_risk.h"
#include "structs/time_series.h"
#include "test_helpers.h"

using test_helpers::make_series;
using test_helpers::make_var_context;

TEST(VaRTest, NinetyPercentConfidenceMatchesHandComputedIndex)
{
    // returns (already ascending) = {-0.05, -0.03, -0.02, -0.01, 0.00, 0.01, 0.02, 0.03, 0.04, 0.05}
    // confidence_level = 0.90 -> index = floor((1 - 0.90) * 10) = floor(1.0) = 1
    // sorted_returns[1] = -0.03
    TimeSeries series = make_series({-0.05, -0.03, -0.02, -0.01, 0.00, 0.01, 0.02, 0.03, 0.04, 0.05});
    VaR metric;

    EXPECT_NEAR(metric.compute(make_var_context(series, 0.90)), -0.03, 1e-12);
}

TEST(VaRTest, NinetyFivePercentConfidenceReturnsWorstValue)
{
    // Same dataset, confidence_level = 0.95 -> index = floor(0.05 * 10) = 0
    // sorted_returns[0] = -0.05 (the single worst historical day)
    TimeSeries series = make_series({-0.05, -0.03, -0.02, -0.01, 0.00, 0.01, 0.02, 0.03, 0.04, 0.05});
    VaR metric;

    EXPECT_NEAR(metric.compute(make_var_context(series, 0.95)), -0.05, 1e-12);
}

TEST(VaRTest, SortsInternallyRegardlessOfInsertionOrder)
{
    // Same values as the first test, but inserted out of order — confirms VaR
    // sorts internally rather than relying on the series already being sorted.
    TimeSeries series = make_series({0.02, -0.01, 0.05, -0.05, 0.00, -0.03, 0.04, 0.01, -0.02, 0.03});
    VaR metric;

    EXPECT_NEAR(metric.compute(make_var_context(series, 0.90)), -0.03, 1e-12);
}

TEST(VaRTest, SinglePointSeriesAlwaysReturnsThatPoint)
{
    // With n=1, the computed index is always 0 regardless of confidence_level
    // (since confidence_level is strictly in (0,1), (1-confidence_level)*1 < 1,
    // which floors to 0) — the only value available is necessarily "the" VaR.
    TimeSeries series = make_series({-0.07});
    VaR metric;

    EXPECT_NEAR(metric.compute(make_var_context(series, 0.99)), -0.07, 1e-12);
}

TEST(VaRTest, ThrowsOnEmptySeries)
{
    TimeSeries series; // no points added
    VaR metric;

    EXPECT_THROW(metric.compute(make_var_context(series, 0.95)), std::runtime_error);
}

TEST(VaRTest, ThrowsOnInvalidConfidenceLevel)
{
    TimeSeries series = make_series({-0.02, -0.01, 0.01, 0.02});
    VaR metric;

    EXPECT_THROW(metric.compute(make_var_context(series, 0.0)), std::invalid_argument);
    EXPECT_THROW(metric.compute(make_var_context(series, 1.0)), std::invalid_argument);
    EXPECT_THROW(metric.compute(make_var_context(series, -0.1)), std::invalid_argument);
    EXPECT_THROW(metric.compute(make_var_context(series, 1.5)), std::invalid_argument);
}