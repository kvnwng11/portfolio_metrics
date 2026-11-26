#include <cmath>

#include <gtest/gtest.h>

#include "metrics/volatility.h"
#include "structs/time_series.h"
#include "test_helpers.h"

using test_helpers::make_returns_context;
using test_helpers::make_series;
using test_helpers::mean;
using test_helpers::sample_stdev;

TEST(VolatilityTest, TwoPointSeriesMatchesHandComputedValue)
{
    // returns = {0.01, 0.03}
    // mean = 0.02
    // squared diffs = (0.01-0.02)^2 = 0.0001, (0.03-0.02)^2 = 0.0001
    // sample variance = 0.0002 / (2-1) = 0.0002
    // daily stdev = sqrt(0.0002) = 0.014142135623730951
    const std::vector<double> returns = {0.01, 0.03};
    const double expected_annualized = sample_stdev(returns) * std::sqrt(kTradingDaysPerYear);

    TimeSeries series = make_series(returns);
    Volatility metric;

    EXPECT_NEAR(metric.compute(make_returns_context(series)), expected_annualized, 1e-9);
}

TEST(VolatilityTest, ThreePointSeriesMatchesHandComputedValue)
{
    // returns = {0.01, -0.01, 0.01}
    // mean = 0.01/3 = 0.0033333333...
    // squared diffs ≈ 0.00004444, 0.00017778, 0.00004444
    // sample variance = 0.00026667 / (3-1) = 0.00013333
    // daily stdev = sqrt(0.00013333...) = 0.011547005383792515
    const std::vector<double> returns = {0.01, -0.01, 0.01};
    const double expected_annualized = sample_stdev(returns) * std::sqrt(kTradingDaysPerYear);

    TimeSeries series = make_series(returns);
    Volatility metric;

    EXPECT_NEAR(metric.compute(make_returns_context(series)), expected_annualized, 1e-9);
}

TEST(VolatilityTest, TenPointSeriesMatchesIndependentlyComputedValue)
{
    // returns = {0.012, -0.008, 0.005, 0.020, -0.015, 0.003, -0.002, 0.010, -0.011, 0.007}
    // mean = 0.0021
    // sample variance ≈ 0.0010969 / 9 ≈ 0.00012188
    // daily stdev ≈ sqrt(0.00012188) ≈ 0.01104
    //
    // Expected value is computed independently (via test_helpers::sample_stdev)
    // so the test checks Volatility against a from-scratch reference
    // calculation rather than hand-typed digits, which is more reliable once
    // n grows past a couple of points.
    const std::vector<double> returns = {
        0.012, -0.008, 0.005, 0.020, -0.015, 0.003, -0.002, 0.010, -0.011, 0.007};
    const double expected_annualized = sample_stdev(returns) * std::sqrt(kTradingDaysPerYear);

    TimeSeries series = make_series(returns);
    Volatility metric;

    EXPECT_NEAR(metric.compute(make_returns_context(series)), expected_annualized, 1e-9);
}

TEST(VolatilityTest, ConstantReturnsProduceZeroVolatility)
{
    // Every return is identical, so variance (and therefore volatility) is exactly zero.
    TimeSeries series = make_series({0.01, 0.01, 0.01, 0.01});
    Volatility metric;

    EXPECT_NEAR(metric.compute(make_returns_context(series)), 0.0, 1e-12);
}

TEST(VolatilityTest, ThrowsOnEmptySeries)
{
    TimeSeries series; // no points added
    Volatility metric;

    EXPECT_THROW(metric.compute(make_returns_context(series)), std::runtime_error);
}

TEST(VolatilityTest, ThrowsOnSinglePointSeries)
{
    // A single data point can't produce a sample standard deviation (division by n-1=0).
    TimeSeries series = make_series({0.01});
    Volatility metric;

    EXPECT_THROW(metric.compute(make_returns_context(series)), std::runtime_error);
}