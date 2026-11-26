#include "time_series.h"

#pragma once

struct MarketContext
{
    const TimeSeries &portfolio_value;   // daily
    const TimeSeries &portfolio_returns; // daily returns with cash flow effects stripped out
    const TimeSeries &benchmark_returns; // daily
    double risk_free_rate;
    double minimum_acceptable_return;
    double confidence_level;
};