#include "datalayer.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <format>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <string_view>
#include <system_error>

namespace
{
    std::string trim(std::string_view text)
    {
        constexpr std::string_view kWhitespace = " \t\r\n";
        const size_t first = text.find_first_not_of(kWhitespace);
        if (first == std::string_view::npos)
        {
            return "";
        }
        const size_t last = text.find_last_not_of(kWhitespace);
        return std::string(text.substr(first, last - first + 1));
    }

    std::string to_lower(std::string text)
    {
        std::transform(text.begin(), text.end(), text.begin(),
                       [](unsigned char c)
                       { return static_cast<char>(std::tolower(c)); });
        return text;
    }

    // Plain comma split. No quoted-field support: tickers and numbers never contain commas.
    std::vector<std::string> split_csv_line(std::string_view line)
    {
        std::vector<std::string> fields;
        size_t start = 0;
        while (true)
        {
            const size_t comma = line.find(',', start);
            if (comma == std::string_view::npos)
            {
                fields.push_back(trim(line.substr(start)));
                break;
            }
            fields.push_back(trim(line.substr(start, comma - start)));
            start = comma + 1;
        }
        return fields;
    }

    double parse_double(const std::string &text, const std::string &where)
    {
        double value = 0.0;
        const char *begin = text.data();
        const char *end = begin + text.size();
        const auto [ptr, ec] = std::from_chars(begin, end, value);
        if (text.empty() || ec != std::errc() || ptr != end || !std::isfinite(value))
        {
            throw std::runtime_error(where + ": invalid number '" + text + "'");
        }
        return value;
    }

    // Accepts "YYYY-MM-DD", optionally followed by a time ("2024-01-02 00:00:00" or
    // "2024-01-02T00:00:00"), which some price sources append.
    Date parse_day(const std::string &text, const std::string &where)
    {
        const bool shape_ok = text.size() >= 10 &&
                              (text.size() == 10 || text[10] == ' ' || text[10] == 'T');
        if (shape_ok)
        {
            try
            {
                const std::string_view day_text = std::string_view(text).substr(0, 10);
                const Date date = internal::parse_date(day_text);
                // Round-trip check: some parsers normalise an impossible date instead of
                // rejecting it (2026-02-29 -> 2026-03-01), which would silently move a trade.
                if (std::format("{:%Y-%m-%d}", date) == day_text)
                {
                    return date;
                }
            }
            catch (const std::invalid_argument &)
            {
                // fall through to the error below, which adds file/line context
            }
        }
        throw std::runtime_error(where + ": invalid date '" + text + "' (expected YYYY-MM-DD)");
    }

    // Reads a CSV file, locates the required columns by header name, and calls on_row with
    // just those columns' values (in the order of required_columns) for every data row.
    void read_csv(const std::filesystem::path &path,
                  const std::vector<std::string> &required_columns,
                  const std::function<void(const std::vector<std::string> &, const std::string &)> &on_row)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            throw std::runtime_error("cannot open file: " + path.string());
        }

        std::string line;
        size_t line_number = 0;
        bool header_seen = false;
        std::vector<size_t> column_index;
        size_t min_fields = 0;

        while (std::getline(file, line))
        {
            ++line_number;

            // Excel / PowerShell can prepend a UTF-8 byte-order mark.
            if (line_number == 1 && line.compare(0, 3, "\xEF\xBB\xBF") == 0)
            {
                line.erase(0, 3);
            }
            if (trim(line).empty())
            {
                continue;
            }

            const std::string where = path.string() + ":" + std::to_string(line_number);
            const std::vector<std::string> fields = split_csv_line(line);

            if (!header_seen)
            {
                for (const std::string &column : required_columns)
                {
                    const auto it = std::find_if(fields.begin(), fields.end(),
                                                 [&](const std::string &field)
                                                 { return to_lower(field) == column; });
                    if (it == fields.end())
                    {
                        throw std::runtime_error(where + ": missing required column '" + column + "'");
                    }
                    const size_t index = static_cast<size_t>(it - fields.begin());
                    column_index.push_back(index);
                    min_fields = std::max(min_fields, index + 1);
                }
                header_seen = true;
                continue;
            }

            if (fields.size() < min_fields)
            {
                throw std::runtime_error(where + ": expected at least " + std::to_string(min_fields) +
                                         " columns, found " + std::to_string(fields.size()));
            }

            std::vector<std::string> values;
            values.reserve(column_index.size());
            for (size_t index : column_index)
            {
                values.push_back(fields[index]);
            }
            on_row(values, where);
        }

        if (!header_seen)
        {
            throw std::runtime_error(path.string() + ": file is empty (no header row)");
        }
    }
}

namespace datalayer
{
    std::vector<Trade> load_trades(const std::filesystem::path &path)
    {
        std::vector<Trade> trades;
        read_csv(path, {"day", "ticker", "quantity"},
                 [&](const std::vector<std::string> &values, const std::string &where)
                 {
                     if (values[1].empty())
                     {
                         throw std::runtime_error(where + ": empty ticker");
                     }
                     trades.push_back(Trade{parse_day(values[0], where), values[1], parse_double(values[2], where)});
                 });
        return trades;
    }

    std::vector<PriceBar> load_prices(const std::filesystem::path &path)
    {
        std::vector<PriceBar> bars;
        read_csv(path, {"day", "ticker", "open_price", "close_price"},
                 [&](const std::vector<std::string> &values, const std::string &where)
                 {
                     if (values[1].empty())
                     {
                         throw std::runtime_error(where + ": empty ticker");
                     }
                     const double open_price = parse_double(values[2], where);
                     const double close_price = parse_double(values[3], where);
                     if (open_price <= 0.0 || close_price <= 0.0)
                     {
                         throw std::runtime_error(where + ": prices must be positive");
                     }
                     bars.push_back(PriceBar{parse_day(values[0], where), values[1], open_price, close_price});
                 });
        return bars;
    }
}