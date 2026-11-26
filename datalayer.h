#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "structs/time_series.h" // for Date

struct Trade
{
    Date day;
    std::string ticker;
    double quantity; // positive = buy, negative = sell
};

struct PriceBar
{
    Date day;
    std::string ticker;
    double open_price;
    double close_price;
};

namespace datalayer
{
    // Expects a header row containing day, ticker, quantity (any order, extra columns
    // ignored, names case-insensitive). Throws std::runtime_error with "file:line" context
    // on any unreadable file, missing column, or malformed value.
    std::vector<Trade> load_trades(const std::filesystem::path &path);

    // Expects a header row containing day, ticker, open_price, close_price — the schema
    // written by scripts/price_scraper.py. Works for both the portfolio prices file and
    // the benchmark file. Prices must be finite and positive.
    std::vector<PriceBar> load_prices(const std::filesystem::path &path);
}