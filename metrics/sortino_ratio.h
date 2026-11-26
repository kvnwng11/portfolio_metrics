#pragma once

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "metric.h"
#include "util.h"
#include "structs/market_context.h"

class SortinoRatio : public Metric
{
public:
    SortinoRatio() = default;

    // Downside deviation follows Sortino & van der Meer's original definition:
    //   sqrt( sum_over_ALL_returns( min(0, r_i - MAR)^2 ) / N )
    // where N is the TOTAL number of return observations, not just the count
    // of returns below MAR. If a given return is above MAR, it is replaced with 0
    // in the calculation.
    double compute(const MarketContext &market_context) const override
    {
        const TimeSeries &daily_returns = market_context.portfolio_returns;
        const size_t n = daily_returns.size();

        if (n < 2)
        {
            throw std::runtime_error("SortinoRatio::compute: need at least 2 data points");
        }

        const double minimum_acceptable_return = market_context.minimum_acceptable_return;

        // Compute annualzied downside deviation
        double squared_sum = 0.0;
        for (const auto &data : daily_returns)
        {
            squared_sum += std::pow(std::min(0.0, data.value - minimum_acceptable_return), 2);
        }
        double daily_downside_deviation = std::sqrt(squared_sum / static_cast<double>(n));
        double annualized_downside_deviation = daily_downside_deviation * std::sqrt(kTradingDaysPerYear);

        if (annualized_downside_deviation == 0.0)
        {
            throw std::runtime_error("SortinoRatio::compute: downside deviation is zero, cannot compute ratio");
        }

        const double mean_daily_return = util::compute_mean(daily_returns);
        const double annualized_return = mean_daily_return * kTradingDaysPerYear;

        return (annualized_return - minimum_acceptable_return) / annualized_downside_deviation;
    }
};
