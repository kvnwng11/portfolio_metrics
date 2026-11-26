#pragma once

#include <chrono>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <vector>

using Date = std::chrono::sys_days;

namespace internal
{
    inline Date parse_date(std::string_view date_str)
    {
        std::istringstream ss{std::string(date_str)};
        Date date;
        ss >> std::chrono::parse("%Y-%m-%d", date);

        if (ss.fail() || (ss >> std::ws && !ss.eof()))
        {
            throw std::invalid_argument("Failed to parse date string. Format must be yyyy-mm-dd");
        }
        return date;
    }
}

struct DataPoint
{
    Date date;
    double value;
};

class TimeSeries
{
public:
    void add_point(std::string_view date_str, double value)
    {
        add_point(internal::parse_date(date_str), value);
    }

    void add_point(Date date, double value)
    {
        data_.push_back({date, value});
    }

    // Binary search lookup by date string: O(log N)
    double get_value_on(std::string_view date_str) const
    {
        Date target = internal::parse_date(date_str);

        auto it = std::lower_bound(data_.begin(), data_.end(), target,
                                   [](const DataPoint &dp, const Date &val)
                                   {
                                       return dp.date < val;
                                   });

        if (it != data_.end() && it->date == target)
        {
            return it->value;
        }
        throw std::out_of_range("Date not found in time series.");
    }

    size_t size() const { return data_.size(); }

    bool empty() const { return data_.empty(); }

    auto begin() const { return data_.begin(); }

    auto end() const { return data_.end(); }

    auto front() const { return data_.front(); }

    auto back() const { return data_.back(); }

    const DataPoint &operator[](size_t i) const { return data_[i]; }

private:
    // Assume data_ is always sorted by date ascending
    std::vector<DataPoint> data_;
};
