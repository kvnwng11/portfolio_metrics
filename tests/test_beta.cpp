#include <gtest/gtest.h>

#include "metrics/beta.h"
#include "structs/time_series.h"
#include "test_helpers.h"

using test_helpers::make_beta_context;
using test_helpers::make_series;
using test_helpers::sample_covariance;
using test_helpers::sample_variance;

TEST(BetaTest, PortfolioScaledFromBenchmarkMatchesExactMultiplier)
{
    // portfolio = 1.5 * benchmark (no noise), so portfolio moves in perfect
    // lockstep with the benchmark, scaled by 1.5x. Beta should come out to
    // exactly 1.5, since cov(1.5y, y) / var(y) = 1.5 * var(y) / var(y) = 1.5.
    const std::vector<double> benchmark_returns = {0.01, -0.02, 0.03, 0.00, -0.01};
    std::vector<double> portfolio_returns;
    for (double r : benchmark_returns)
        portfolio_returns.push_back(1.5 * r);

    TimeSeries portfolio = make_series(portfolio_returns);
    TimeSeries benchmark = make_series(benchmark_returns);
    Beta metric;

    EXPECT_NEAR(metric.compute(make_beta_context(portfolio, benchmark)), 1.5, 1e-9);
}

TEST(BetaTest, PortfolioInverselyScaledFromBenchmarkMatchesNegativeMultiplier)
{
    // Same idea, but with a negative multiplier, confirming negative beta
    // (portfolio moves opposite the benchmark) isn't treated as degenerate.
    const std::vector<double> benchmark_returns = {0.02, -0.01, 0.015, -0.03, 0.005};
    std::vector<double> portfolio_returns;
    for (double r : benchmark_returns)
        portfolio_returns.push_back(-0.5 * r);

    TimeSeries portfolio = make_series(portfolio_returns);
    TimeSeries benchmark = make_series(benchmark_returns);
    Beta metric;

    EXPECT_NEAR(metric.compute(make_beta_context(portfolio, benchmark)), -0.5, 1e-9);
}

TEST(BetaTest, BenchmarkComparedWithItselfHasBetaOfOne)
{
    // cov(x, x) == var(x), so beta of any series against itself is exactly 1.0.
    TimeSeries series = make_series({0.01, -0.02, 0.03, 0.015, -0.01});
    Beta metric;

    EXPECT_NEAR(metric.compute(make_beta_context(series, series)), 1.0, 1e-9);
}

TEST(BetaTest, MatchesIndependentlyComputedValueOnUncorrelatedData)
{
    // Dataset with no simple exact relationship between the two series — checks
    // Beta against a from-scratch reference calculation rather than a
    // specially-constructed exact-multiple case.
    const std::vector<double> portfolio_returns = {0.012, -0.008, 0.005, 0.020, -0.015};
    const std::vector<double> benchmark_returns = {0.010, -0.005, 0.008, 0.012, -0.010};

    const double expected_beta =
        sample_covariance(portfolio_returns, benchmark_returns) / sample_variance(benchmark_returns);

    TimeSeries portfolio = make_series(portfolio_returns);
    TimeSeries benchmark = make_series(benchmark_returns);
    Beta metric;

    EXPECT_NEAR(metric.compute(make_beta_context(portfolio, benchmark)), expected_beta, 1e-9);
}

TEST(BetaTest, ThrowsOnZeroBenchmarkVariance)
{
    // Benchmark returns are constant, so var(benchmark) == 0.
    TimeSeries portfolio = make_series({0.01, -0.02, 0.03, 0.01});
    TimeSeries benchmark = make_series({0.01, 0.01, 0.01, 0.01});
    Beta metric;

    EXPECT_THROW(metric.compute(make_beta_context(portfolio, benchmark)), std::runtime_error);
}

TEST(BetaTest, ThrowsOnMismatchedSeriesLengths)
{
    TimeSeries portfolio = make_series({0.01, -0.02, 0.03});
    TimeSeries benchmark = make_series({0.01, -0.02});
    Beta metric;

    EXPECT_THROW(metric.compute(make_beta_context(portfolio, benchmark)), std::invalid_argument);
}

TEST(BetaTest, ThrowsOnInsufficientData)
{
    TimeSeries portfolio = make_series({0.01});
    TimeSeries benchmark = make_series({0.01});
    Beta metric;

    EXPECT_THROW(metric.compute(make_beta_context(portfolio, benchmark)), std::runtime_error);
}