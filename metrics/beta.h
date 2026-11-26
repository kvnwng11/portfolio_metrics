#pragma once

#include <stdexcept>

#include "metric.h"
#include "util.h"
#include "structs/market_context.h"

class Beta : public Metric
{
public:
    Beta() = default;

    // Computed directly as cov(portfolio_returns, benchmark_returns) / var(benchmark_returns),
    // rather than as a weighted average of per-asset betas.
    //
    // Relies on util::compute_cov / util::compute_var for insufficient-data and
    // date-alignment validation (see util.h) — Beta itself doesn't duplicate those
    // checks, only guards the case specific to this ratio: zero benchmark variance.
    double compute(const MarketContext &market_context) const override
    {
        const double benchmark_variance = util::compute_var(market_context.benchmark_returns);

        if (benchmark_variance == 0.0)
        {
            throw std::runtime_error("Beta::compute: benchmark variance is zero, cannot compute beta");
        }

        const double covariance = util::compute_cov(market_context.portfolio_returns, market_context.benchmark_returns);
        return covariance / benchmark_variance;
    }
};