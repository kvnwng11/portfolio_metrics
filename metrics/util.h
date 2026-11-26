#pragma once

#include <cmath>
#include <stdexcept>
#include <string>

#include "structs/time_series.h"

constexpr double kTradingDaysPerYear = 252.0;

namespace util
{
    inline double compute_mean(const TimeSeries &series)
    {
        size_t n = series.size();
        double sum = 0.0;
        for (const auto &point : series)
        {
            sum += point.value;
        }
        return sum / static_cast<double>(n);
    }

    inline double compute_stdev(const TimeSeries &series)
    {
        size_t n = series.size();
        const double mean = compute_mean(series);
        double sum_sq_diff = 0.0;
        for (const auto &point : series)
        {
            sum_sq_diff += std::pow(point.value - mean, 2);
        }

        // Sample standard deviation (Bessel's correction), daily (not annualized).
        return std::sqrt(sum_sq_diff / static_cast<double>(n - 1));
    }

    inline double compute_var(const TimeSeries &series)
    {
        // Sample variance
        return std::pow(compute_stdev(series), 2);
    }

    inline double compute_cov(const TimeSeries &series1, const TimeSeries &series2)
    {

        const size_t n = series1.size();

        if (series2.size() != n)
        {
            throw std::invalid_argument("compute_cov: series must have equal length");
        }
        if (n < 2)
        {
            throw std::runtime_error("compute_cov: need at least 2 data points");
        }

        const double mean1 = compute_mean(series1);
        const double mean2 = compute_mean(series2);

        double sum = 0.0;
        for (size_t i = 0; i < n; ++i)
        {
            if (series1[i].date != series2[i].date)
            {
                throw std::invalid_argument("compute_cov: series are not date-aligned at index " + std::to_string(i));
            }
            sum += (series1[i].value - mean1) * (series2[i].value - mean2);
        }

        // Sample covariance
        return sum / static_cast<double>(n - 1);
    }
};