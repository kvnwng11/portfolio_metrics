#include <cmath>

#include <gtest/gtest.h>

#include "metrics/sortino_ratio.h"
#include "structs/time_series.h"
#include "test_helpers.h"

using test_helpers::downside_deviation;
using test_helpers::make_series;
using test_helpers::make_sortino_context;
using test_helpers::mean;

TEST(SortinoRatioTest, MixedReturnsMatchesIndependentlyComputedValue)
{
    // returns = {0.03, -0.01, 0.02, -0.03, 0.01}, MAR = 0.0
    // downside contributions: 0, -0.01, 0, -0.03, 0  (positive returns contribute 0)
    // sum of squares = 0.0001 + 0.0009 = 0.0010, N = 5 (total count, not just below-MAR)
    // daily downside deviation = sqrt(0.0010 / 5) = sqrt(0.0002) ≈ 0.014142
    // mean daily return = 0.02 / 5 = 0.004, annualized = 0.004 * 252 = 1.008
    const std::vector<double> returns = {0.03, -0.01, 0.02, -0.03, 0.01};
    const double mar = 0.0;

    const double annualized_return = mean(returns) * kTradingDaysPerYear;
    const double annualized_downside_dev = downside_deviation(returns, mar) * std::sqrt(kTradingDaysPerYear);
    const double expected_sortino = (annualized_return - mar) / annualized_downside_dev;

    TimeSeries series = make_series(returns);
    SortinoRatio metric;

    EXPECT_NEAR(metric.compute(make_sortino_context(series, mar)), expected_sortino, 1e-9);
}

TEST(SortinoRatioTest, EqualAnnualizedReturnAndMarProducesZeroSortino)
{
    // returns = {0.02, -0.01, 0.015, -0.03, 0.005}, MAR = 0.0
    // mean daily return happens to be exactly 0, so annualized_return == MAR == 0,
    // making the numerator (and therefore the ratio) exactly zero regardless of
    // the (nonzero) downside deviation in the denominator.
    const std::vector<double> returns = {0.02, -0.01, 0.015, -0.03, 0.005};
    const double mar = 0.0;

    ASSERT_NEAR(mean(returns), 0.0, 1e-12); // sanity-check the fixture itself

    TimeSeries series = make_series(returns);
    SortinoRatio metric;

    EXPECT_NEAR(metric.compute(make_sortino_context(series, mar)), 0.0, 1e-9);
}

TEST(SortinoRatioTest, NoReturnsBelowMarThrowsOnZeroDownsideDeviation)
{
    // MAR set far below every return in the series, so every day clears the
    // bar: downside deviation is exactly 0, which should be treated as a
    // degenerate input (division by zero) rather than silently returning inf.
    TimeSeries series = make_series({0.01, 0.02, 0.03});
    SortinoRatio metric;

    EXPECT_THROW(metric.compute(make_sortino_context(series, -1.0)), std::runtime_error);
}

TEST(SortinoRatioTest, NegativeExcessReturnProducesNegativeSortino)
{
    // A MAR higher than the portfolio's annualized return should produce a
    // negative Sortino ratio — a valid (if unflattering) result, not an error.
    const std::vector<double> returns = {0.001, -0.002, 0.0015, -0.001};
    const double mar = 0.10; // deliberately high relative to tiny daily returns

    const double annualized_return = mean(returns) * kTradingDaysPerYear;
    const double annualized_downside_dev = downside_deviation(returns, mar) * std::sqrt(kTradingDaysPerYear);
    const double expected_sortino = (annualized_return - mar) / annualized_downside_dev;

    ASSERT_LT(expected_sortino, 0.0); // sanity-check the fixture is set up as intended

    TimeSeries series = make_series(returns);
    SortinoRatio metric;

    EXPECT_NEAR(metric.compute(make_sortino_context(series, mar)), expected_sortino, 1e-9);
}

TEST(SortinoRatioTest, ThrowsOnEmptySeries)
{
    TimeSeries series; // no points added
    SortinoRatio metric;

    EXPECT_THROW(metric.compute(make_sortino_context(series, 0.0)), std::runtime_error);
}

TEST(SortinoRatioTest, ThrowsOnSinglePointSeries)
{
    // Note: the downside-deviation formula itself doesn't strictly require
    // n >= 2 (there's no n-1 term), but SortinoRatio enforces the same minimum
    // as the other metrics for consistency. This test documents that choice.
    TimeSeries series = make_series({0.01});
    SortinoRatio metric;

    EXPECT_THROW(metric.compute(make_sortino_context(series, 0.0)), std::runtime_error);
}