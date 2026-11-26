#pragma once

#include <vector>

#include "datalayer.h"
#include "structs/time_series.h"

// Rebuilds a portfolio's daily value and cash-flow-adjusted (time-weighted) returns from a
// trade log and daily prices. This is the one place cash-flow adjustment happens; every
// return-based metric inherits it by consuming returns().
//
// Modelling conventions (assumptions about the data, not facts):
//   * No cash balance is modelled. Every trade is an external cash flow: a buy is a
//     deposit, a sell is a withdrawal. value() is the market value of holdings only.
//   * A trade executes at the closing price of the day it is dated. A trade dated on a day
//     with no price data (weekend, holiday) executes at the next trading day's close.
//   * Long-only: selling more than is held throws.
//   * Trading calendar = union of dates on which any traded ticker has a price bar. If a
//     held ticker has no bar on a calendar day, its last known close is carried forward.
//   * Daily return r_t = (yesterday's holdings repriced at today's close) / V_{t-1} - 1.
//     Today's trades never touch r_t, which makes this an exact time-weighted return when
//     trades execute at the close. Days where V_{t-1} == 0 (before the first buy, or after
//     a full liquidation) have no return and are omitted from returns().
//   * PriceBar::open_price is not used under these conventions.
//
// Lifetime: a MarketContext built from value()/returns() holds references into this object,
// so the Portfolio must outlive it.
class Portfolio
{
public:
    // Throws std::invalid_argument for structurally bad input (no trades, a traded ticker
    // with no price data, duplicate bars, a trade outside the price data's date range) and
    // std::runtime_error for semantic problems (overselling, no price available for a
    // held ticker).
    Portfolio(const std::vector<Trade> &trades, const std::vector<PriceBar> &prices);

    // Market value of holdings at each close, from the first trade's day onward.
    const TimeSeries &value() const { return value_; }

    // Cash-flow-adjusted daily returns (see conventions above).
    const TimeSeries &returns() const { return returns_; }

    // Benchmark close-to-close returns on exactly the dates of returns(), so the two series
    // are date-aligned for covariance/beta. Each return uses the previous trading day in
    // this portfolio's calendar as its base. A calendar day on which the benchmark has no
    // bar is forward-filled (a zero return for that day).
    // Throws std::invalid_argument if the data is empty, has several tickers, or has
    // duplicate days; std::runtime_error if the benchmark has no price early enough.
    TimeSeries benchmark_returns(const std::vector<PriceBar> &benchmark_prices) const;

private:
    std::vector<Date> calendar_;
    TimeSeries value_;
    TimeSeries returns_;
};