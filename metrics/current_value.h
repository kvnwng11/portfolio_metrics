#pragma once

#include "metric.h"
#include "structs/market_context.h"

class CurrentValue : public Metric
{
public:
    CurrentValue() = default;

    double compute(const MarketContext &market_context) const override
    {
        const TimeSeries &portfolio_value = market_context.portfolio_value;
        if (portfolio_value.empty())
        {
            throw std::runtime_error("Error: empty portfolio");
        }
        return portfolio_value.back().value;
    }
};