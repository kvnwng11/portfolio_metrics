#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "structs/market_context.h"
#include "structs/time_series.h"

// Shared fixtures for metric unit tests. inline is required here (not just
// a style choice) since this header is included by multiple test .cpp files
// that get linked into one binary — without it, each translation unit would
// emit its own external definition of these functions, causing a
// "already defined" link error.

namespace test_helpers
{
    // A single shared empty series, used to fill MarketContext fields that a
    // given metric under test doesn't read. Declared static so every
    // MarketContext built from these helpers refers to the same long-lived
    // instance rather than a temporary that would dangle after the call returns.
    inline const TimeSeries &empty_series()
    {
        static TimeSeries series;
        return series;
    }

    inline MarketContext make_value_context(const TimeSeries &portfolio_value)
    {
        return MarketContext{
            portfolio_value,
            empty_series(), // portfolio_returns
            empty_series(), // benchmark_returns
            0.0,            // risk_free_rate
            0.0,            // minimum_acceptable_return
            0.95            // confidence_level
        };
    }

    inline MarketContext make_returns_context(const TimeSeries &portfolio_returns)
    {
        return MarketContext{
            empty_series(), // portfolio_value
            portfolio_returns,
            empty_series(), // benchmark_returns
            0.0,            // risk_free_rate
            0.0,            // minimum_acceptable_return
            0.95            // confidence_level
        };
    }

    inline MarketContext make_sharpe_context(const TimeSeries &portfolio_returns, double risk_free_rate = 0.0)
    {
        return MarketContext{
            empty_series(), // portfolio_value
            portfolio_returns,
            empty_series(), // benchmark_returns
            risk_free_rate,
            0.0, // minimum_acceptable_return
            0.95 // confidence_level
        };
    }

    inline MarketContext make_sortino_context(const TimeSeries &portfolio_returns, double minimum_acceptable_return = 0.0)
    {
        return MarketContext{
            empty_series(), // portfolio_value
            portfolio_returns,
            empty_series(), // benchmark_returns
            0.0,            // risk_free_rate
            minimum_acceptable_return,
            0.95 // confidence_level
        };
    }

    inline MarketContext make_beta_context(const TimeSeries &portfolio_returns, const TimeSeries &benchmark_returns)
    {
        return MarketContext{
            empty_series(), // portfolio_value
            portfolio_returns,
            benchmark_returns,
            0.0, // risk_free_rate
            0.0, // minimum_acceptable_return
            0.95 // confidence_level
        };
    }

    inline MarketContext make_var_context(const TimeSeries &portfolio_returns, double confidence_level = 0.95)
    {
        return MarketContext{
            empty_series(), // portfolio_value
            portfolio_returns,
            empty_series(), // benchmark_returns
            0.0,            // risk_free_rate
            0.0,            // minimum_acceptable_return
            confidence_level};
    }

    // Reference implementations used to compute expected values independently
    // in tests, rather than hand-typing hard-to-verify decimal literals.
    inline double mean(const std::vector<double> &values)
    {
        double sum = 0.0;
        for (double v : values)
            sum += v;
        return sum / static_cast<double>(values.size());
    }

    inline double sample_stdev(const std::vector<double> &values)
    {
        const double m = mean(values);
        double sum_sq = 0.0;
        for (double v : values)
            sum_sq += (v - m) * (v - m);
        return std::sqrt(sum_sq / static_cast<double>(values.size() - 1));
    }

    inline double sample_covariance(const std::vector<double> &a, const std::vector<double> &b)
    {
        const double ma = mean(a);
        const double mb = mean(b);
        double sum = 0.0;
        for (size_t i = 0; i < a.size(); ++i)
            sum += (a[i] - ma) * (b[i] - mb);
        return sum / static_cast<double>(a.size() - 1);
    }

    inline double sample_variance(const std::vector<double> &values)
    {
        const double s = sample_stdev(values);
        return s * s;
    }

    // Reference downside deviation: sqrt(sum(min(0, v - mar)^2) / N), N = total count.
    inline double downside_deviation(const std::vector<double> &values, double mar)
    {
        double sum_sq = 0.0;
        for (double v : values)
        {
            const double downside = std::min(0.0, v - mar);
            sum_sq += downside * downside;
        }
        return std::sqrt(sum_sq / static_cast<double>(values.size()));
    }

    inline TimeSeries make_series(const std::vector<double> &values)
    {
        TimeSeries series;
        // Dates are arbitrary/sequential here — most metrics only consume
        // values, not dates, so exact dates don't matter for these fixtures.
        int day = 1;
        for (double v : values)
        {
            char buf[11];
            std::snprintf(buf, sizeof(buf), "2024-01-%02d", day++);
            series.add_point(buf, v);
        }
        return series;
    }
}