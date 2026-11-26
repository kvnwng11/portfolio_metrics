#pragma once

#include <stdexcept>

#include "metric.h"
#include "util.h"
#include "structs/market_context.h"
#include "volatility.h"

class SharpeRatio : public Metric
{
public:
    SharpeRatio() = default;

    // For finding average daily returns, use arithmetic mean instead of geometric mean
    double compute(const MarketContext &market_context) const override
    {
        double annualized_vol = Volatility().compute(market_context);

        if (annualized_vol == 0.0)
        {
            throw std::runtime_error("SharpeRatio::compute: volatility is zero, cannot compute ratio");
        }

        double mean_daily_return = util::compute_mean(market_context.portfolio_returns);
        double annualized_return = mean_daily_return * kTradingDaysPerYear;

        return (annualized_return - market_context.risk_free_rate) / annualized_vol;
    }
};
