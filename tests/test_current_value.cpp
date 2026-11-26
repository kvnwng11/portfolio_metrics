#include <gtest/gtest.h>

#include "metrics/current_value.h"
#include "structs/time_series.h"
#include "test_helpers.h"

using test_helpers::make_series;
using test_helpers::make_value_context;

TEST(CurrentValueTest, ReturnsLastValueInMultiPointSeries)
{
    TimeSeries series = make_series({100.0, 105.0, 110.0});

    CurrentValue metric;
    EXPECT_DOUBLE_EQ(metric.compute(make_value_context(series)), 110.0);
}

TEST(CurrentValueTest, ReturnsOnlyValueInSinglePointSeries)
{
    TimeSeries series = make_series({42.0});

    CurrentValue metric;
    EXPECT_DOUBLE_EQ(metric.compute(make_value_context(series)), 42.0);
}

TEST(CurrentValueTest, ThrowsOnEmptySeriesInsteadOfUnderflowing)
{
    TimeSeries series; // no points added

    CurrentValue metric;
    EXPECT_THROW(metric.compute(make_value_context(series)), std::runtime_error);
}

// KNOWN LIMITATION: this test documents an assumption CurrentValue relies on
// rather than enforces: it returns the LAST-INSERTED point, not the
// chronologically-latest one. If TimeSeries doesn't guarantee sorted-on-insert
// (or a finalize/sort step isn't called), this will silently return the wrong
// "current" value. If TimeSeries is later changed to sort on insert, update
// this assertion to expect 999.0 regardless of insertion order.
TEST(CurrentValueTest, ReflectsInsertionOrderNotChronologicalOrder)
{
    TimeSeries series;
    series.add_point("2024-01-05", 999.0); // inserted first, but chronologically later
    series.add_point("2024-01-01", 100.0); // inserted second, but chronologically earlier

    CurrentValue metric;
    EXPECT_DOUBLE_EQ(metric.compute(make_value_context(series)), 100.0);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}