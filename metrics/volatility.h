#pragma once

#include <cmath>
#include <stdexcept>

#include "metric.h"
#include "util.h"
#include "structs/market_context.h"
#include "structs/time_series.h"

class Volatility : public Metric
{
public:
    Volatility(bool annualize = true) : annualize_(annualize) {};

    double compute(const MarketContext &market_context) const override
    {
        const TimeSeries &daily_returns = market_context.portfolio_returns;
        size_t n = daily_returns.size();
        if (n < 2)
        {
            throw std::runtime_error("Volatility::compute: need at least 2 data points");
        }

        const double daily_stdev = util::compute_stdev(daily_returns);
        return annualize_ ? daily_stdev * std::sqrt(kTradingDaysPerYear) : daily_stdev;
    }

private:
    // If true, returns annualized vol. Else, return daily vol
    bool annualize_;
};