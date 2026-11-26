#pragma once

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "metric.h"
#include "util.h"
#include "structs/market_context.h"

class MaxDrawdown : public Metric
{
public:
    MaxDrawdown() = default;

    // Simulate the growth of a $100,000 portfolio using the portfolio returns
    double compute(const MarketContext &market_context) const override
    {
        const TimeSeries &daily_returns = market_context.portfolio_returns;
        if (daily_returns.empty())
        {
            throw std::runtime_error("MaxDrawdown::compute: no return data available");
        }

        double index = 100000.0;
        double peak = index;
        double max_drawdown = 0.0;
        for (const auto &data : daily_returns)
        {
            index *= (1 + data.value);
            peak = std::max(peak, index);

            const double drawdown = (index - peak) / peak; // always <= 0
            max_drawdown = std::min(max_drawdown, drawdown);
        }
        return max_drawdown;
    }
};
