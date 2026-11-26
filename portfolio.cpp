#include "portfolio.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

namespace
{
    constexpr double kQuantityTolerance = 1e-9;
    constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

    std::string format_day(Date date)
    {
        return std::format("{:%Y-%m-%d}", date);
    }
}

Portfolio::Portfolio(const std::vector<Trade> &trades, const std::vector<PriceBar> &prices)
{
    if (trades.empty())
    {
        throw std::invalid_argument("Portfolio: no trades provided");
    }

    // Give every traded ticker a stable index (alphabetical, via std::map ordering).
    std::map<std::string, size_t> ticker_index;
    for (const Trade &trade : trades)
    {
        ticker_index.emplace(trade.ticker, 0);
    }
    std::vector<std::string> tickers;
    tickers.reserve(ticker_index.size());
    for (auto &[ticker, index] : ticker_index)
    {
        index = tickers.size();
        tickers.push_back(ticker);
    }
    const size_t num_tickers = tickers.size();

    // Collect closing prices for traded tickers. The trading calendar is the union of
    // the dates they have bars on.
    std::vector<std::map<Date, double>> bars(num_tickers);
    std::set<Date> calendar_dates;
    for (const PriceBar &bar : prices)
    {
        const auto it = ticker_index.find(bar.ticker);
        if (it == ticker_index.end())
        {
            continue; // price data for a ticker that was never traded
        }
        if (!bars[it->second].emplace(bar.day, bar.close_price).second)
        {
            throw std::invalid_argument("Portfolio: duplicate price bar for " + bar.ticker + " on " + format_day(bar.day));
        }
        calendar_dates.insert(bar.day);
    }
    for (size_t i = 0; i < num_tickers; ++i)
    {
        if (bars[i].empty())
        {
            throw std::invalid_argument("Portfolio: no price data for ticker " + tickers[i]);
        }
    }
    calendar_.assign(calendar_dates.begin(), calendar_dates.end());
    const size_t num_days = calendar_.size();

    // close[i][d]: ticker i's close on calendar day d, carried forward over gaps (NaN
    // until the ticker's first bar).
    std::vector<std::vector<double>> close(num_tickers, std::vector<double>(num_days, kNaN));
    for (size_t i = 0; i < num_tickers; ++i)
    {
        double last = kNaN;
        for (size_t d = 0; d < num_days; ++d)
        {
            const auto it = bars[i].find(calendar_[d]);
            if (it != bars[i].end())
            {
                last = it->second;
            }
            close[i][d] = last;
        }
    }

    // Assign every trade to the session it executes in: its own day if that is a trading
    // day, otherwise the next one.
    std::vector<std::vector<std::pair<size_t, double>>> trades_by_day(num_days);
    for (const Trade &trade : trades)
    {
        const auto session = std::lower_bound(calendar_.begin(), calendar_.end(), trade.day);
        if (session == calendar_.end())
        {
            throw std::invalid_argument("Portfolio: trade in " + trade.ticker + " on " + format_day(trade.day) +
                                        " is after the last day of price data");
        }
        if (session == calendar_.begin() && *session != trade.day)
        {
            throw std::invalid_argument("Portfolio: trade in " + trade.ticker + " on " + format_day(trade.day) +
                                        " is before the first day of price data");
        }
        trades_by_day[static_cast<size_t>(session - calendar_.begin())].emplace_back(ticker_index.at(trade.ticker), trade.quantity);
    }

    std::vector<double> position(num_tickers, 0.0);

    // Market value of the current positions at day d's closing prices.
    auto mark_to_market = [&](size_t d)
    {
        double total = 0.0;
        for (size_t i = 0; i < num_tickers; ++i)
        {
            if (position[i] == 0.0)
            {
                continue;
            }
            if (std::isnan(close[i][d]))
            {
                throw std::runtime_error("Portfolio: no price available for " + tickers[i] +
                                         " on or before " + format_day(calendar_[d]));
            }
            total += position[i] * close[i][d];
        }
        return total;
    };

    double previous_value = 0.0; // V_{t-1}
    bool started = false;        // true from the first trade's day onward

    for (size_t d = 0; d < num_days; ++d)
    {
        // Yesterday's holdings repriced at today's close; today's trades not applied yet.
        const double value_before_trades = mark_to_market(d);

        if (previous_value > 0.0)
        {
            returns_.add_point(calendar_[d], value_before_trades / previous_value - 1.0);
        }

        for (const auto &[ticker_idx, quantity] : trades_by_day[d])
        {
            position[ticker_idx] += quantity;
        }
        for (const auto &[ticker_idx, quantity] : trades_by_day[d])
        {
            double &held = position[ticker_idx];
            if (std::abs(held) < kQuantityTolerance)
            {
                held = 0.0; // snap floating-point residue to an exact zero position
            }
            else if (held < 0.0)
            {
                throw std::runtime_error("Portfolio: selling more " + tickers[ticker_idx] + " than is held on " +
                                         format_day(calendar_[d]) + " (short positions are not supported)");
            }
        }

        if (!trades_by_day[d].empty())
        {
            started = true;
        }
        const double value = mark_to_market(d);
        if (started)
        {
            value_.add_point(calendar_[d], value);
        }
        previous_value = value;
    }
}

TimeSeries Portfolio::benchmark_returns(const std::vector<PriceBar> &benchmark_prices) const
{
    if (benchmark_prices.empty())
    {
        throw std::invalid_argument("Portfolio::benchmark_returns: no benchmark price data");
    }

    const std::string &benchmark_ticker = benchmark_prices.front().ticker;
    std::map<Date, double> benchmark_close;
    for (const PriceBar &bar : benchmark_prices)
    {
        if (bar.ticker != benchmark_ticker)
        {
            throw std::invalid_argument("Portfolio::benchmark_returns: benchmark data contains more than one ticker ('" +
                                        benchmark_ticker + "' and '" + bar.ticker + "')");
        }
        if (!benchmark_close.emplace(bar.day, bar.close_price).second)
        {
            throw std::invalid_argument("Portfolio::benchmark_returns: duplicate benchmark bar on " + format_day(bar.day));
        }
    }

    // Benchmark close on each calendar day, carried forward over gaps.
    std::vector<double> close(calendar_.size(), kNaN);
    double last = kNaN;
    for (size_t d = 0; d < calendar_.size(); ++d)
    {
        const auto it = benchmark_close.find(calendar_[d]);
        if (it != benchmark_close.end())
        {
            last = it->second;
        }
        close[d] = last;
    }

    TimeSeries result;
    for (const auto &point : returns_)
    {
        // Every return date is in calendar_ and is never its first entry (a return needs
        // a previous day), so d >= 1.
        const auto it = std::lower_bound(calendar_.begin(), calendar_.end(), point.date);
        const size_t d = static_cast<size_t>(it - calendar_.begin());
        if (std::isnan(close[d]) || std::isnan(close[d - 1]))
        {
            throw std::runtime_error("Portfolio::benchmark_returns: benchmark has no price on or before " +
                                     format_day(calendar_[d - 1]));
        }
        result.add_point(point.date, close[d] / close[d - 1] - 1.0);
    }
    return result;
}