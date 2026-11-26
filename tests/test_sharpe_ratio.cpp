#include <cmath>

#include <gtest/gtest.h>

#include "metrics/sharpe_ratio.h"
#include "structs/time_series.h"
#include "test_helpers.h"

using test_helpers::make_series;
using test_helpers::make_sharpe_context;
using test_helpers::mean;
using test_helpers::sample_stdev;

TEST(SharpeRatioTest, TwoPointSeriesMatchesIndependentlyComputedValue)
{
    // returns = {0.01, 0.03}, risk_free_rate = 0.02 (annual)
    // mean_daily_return = 0.02
    // annualized_return = 0.02 * 252 = 5.04
    // daily stdev = sqrt(0.0002) ≈ 0.014142
    // annualized_vol = daily stdev * sqrt(252)
    // sharpe = (5.04 - 0.02) / annualized_vol
    const std::vector<double> returns = {0.01, 0.03};
    const double risk_free_rate = 0.02;

    const double annualized_return = mean(returns) * kTradingDaysPerYear;
    const double annualized_vol = sample_stdev(returns) * std::sqrt(kTradingDaysPerYear);
    const double expected_sharpe = (annualized_return - risk_free_rate) / annualized_vol;

    TimeSeries series = make_series(returns);
    SharpeRatio metric;

    EXPECT_NEAR(metric.compute(make_sharpe_context(series, risk_free_rate)), expected_sharpe, 1e-9);
}

TEST(SharpeRatioTest, ZeroRiskFreeRateMatchesIndependentlyComputedValue)
{
    // Same dataset as above, but with risk_free_rate = 0 — isolates the
    // numerator/denominator computation from the risk-free subtraction.
    const std::vector<double> returns = {0.012, -0.008, 0.005, 0.020, -0.015, 0.003, -0.002, 0.010, -0.011, 0.007};

    const double annualized_return = mean(returns) * kTradingDaysPerYear;
    const double annualized_vol = sample_stdev(returns) * std::sqrt(kTradingDaysPerYear);
    const double expected_sharpe = annualized_return / annualized_vol;

    TimeSeries series = make_series(returns);
    SharpeRatio metric;

    EXPECT_NEAR(metric.compute(make_sharpe_context(series, 0.0)), expected_sharpe, 1e-9);
}

TEST(SharpeRatioTest, ThrowsOnEmptySeries)
{
    TimeSeries series; // no points added
    SharpeRatio metric;

    EXPECT_THROW(metric.compute(make_sharpe_context(series, 0.02)), std::runtime_error);
}

TEST(SharpeRatioTest, ThrowsOnSinglePointSeries)
{
    TimeSeries series = make_series({0.01});
    SharpeRatio metric;

    EXPECT_THROW(metric.compute(make_sharpe_context(series, 0.02)), std::runtime_error);
}

// This test documents a gap flagged in review: SharpeRatio::compute does not
// currently guard against zero volatility, so it will return +/-inf or NaN
// rather than throwing. This test encodes the desired behavior (throw) and
// will fail until that guard is added — treat it as a to-do, not a false
// positive, if it's currently red.
TEST(SharpeRatioTest, ThrowsOnZeroVolatility)
{
    // All returns identical => zero volatility => division by zero in the formula.
    TimeSeries series = make_series({0.01, 0.01, 0.01, 0.01});
    SharpeRatio metric;

    EXPECT_THROW(metric.compute(make_sharpe_context(series, 0.02)), std::runtime_error);
}

TEST(SharpeRatioTest, NegativeExcessReturnProducesNegativeSharpe)
{
    // A risk-free rate higher than the portfolio's annualized return should
    // produce a negative Sharpe ratio, not an error — this isn't a degenerate
    // case, just an unprofitable one relative to the risk-free benchmark.
    const std::vector<double> returns = {0.001, -0.002, 0.0015, -0.001};
    const double risk_free_rate = 0.10; // deliberately high relative to tiny daily returns

    const double annualized_return = mean(returns) * kTradingDaysPerYear;
    const double annualized_vol = sample_stdev(returns) * std::sqrt(kTradingDaysPerYear);
    const double expected_sharpe = (annualized_return - risk_free_rate) / annualized_vol;

    ASSERT_LT(expected_sharpe, 0.0); // sanity-check the test fixture itself is set up as intended

    TimeSeries series = make_series(returns);
    SharpeRatio metric;

    EXPECT_NEAR(metric.compute(make_sharpe_context(series, risk_free_rate)), expected_sharpe, 1e-9);
}