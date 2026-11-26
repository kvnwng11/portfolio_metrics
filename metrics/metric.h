#pragma once

#include "structs/market_context.h"

class Metric
{
public:
    // If there is insufficient or invalid data, throw an InsufficientDataError
    [[nodiscard]] virtual double compute(const MarketContext &market_context) const = 0;
    virtual ~Metric() = default;
};