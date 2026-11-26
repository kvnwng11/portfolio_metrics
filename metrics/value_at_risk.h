#pragma once

#include <algorithm>
#include <stdexcept>
#include <vector>

#include "metric.h"
#include "structs/market_context.h"

class VaR : public Metric
{
public:
    VaR() = default;

    // Historical Value at Risk: sorts daily portfolio returns worst-to-best and
    // returns the return at the (1 - confidence_level) percentile — e.g. at a
    // 95% confidence level, this is the 5th-percentile return: the threshold
    // below which only 5% of historical daily returns fell.
    //
    // Returns a raw return fraction (not a dollar amount), and will typically
    // be negative (a loss). confidence_level is read from MarketContext and
    // must be in (0, 1).
    //
    // Convention note: uses the nearest-rank (floor index) method with no
    // interpolation between adjacent sorted returns — a deliberate choice, not
    // guaranteed to match every other VaR tool's exact percentile definition.
    double compute(const MarketContext &market_context) const override
    {
        const double confidence_level = market_context.confidence_level;
        if (confidence_level <= 0.0 || confidence_level >= 1.0)
        {
            throw std::invalid_argument("VaR::compute: confidence_level must be in (0, 1)");
        }

        const TimeSeries &daily_returns = market_context.portfolio_returns;
        const size_t n = daily_returns.size();
        if (n == 0)
        {
            throw std::runtime_error("VaR::compute: no return data available");
        }

        std::vector<double> sorted_returns;
        sorted_returns.reserve(n);
        for (const auto &point : daily_returns)
        {
            sorted_returns.push_back(point.value);
        }
        std::sort(sorted_returns.begin(), sorted_returns.end());

        // A small epsilon guards against floating-point representation error:
        // e.g. 1.0 - 0.90 doesn't evaluate to exactly 0.1 in double precision,
        // so (1.0 - confidence_level) * n can land a hair under an integer
        // boundary (0.999999999999998 instead of 1.0) and truncate one index
        // too low without this nudge.
        constexpr double kEpsilon = 1e-9;
        size_t index = static_cast<size_t>((1.0 - confidence_level) * static_cast<double>(n) + kEpsilon);
        index = std::min(index, n - 1); // guard against rounding landing on n

        return sorted_returns[index];
    }
};