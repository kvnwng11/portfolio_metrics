#include <gtest/gtest.h>

#include "datalayer.h"
#include "portfolio.h"
#include "structs/time_series.h"

namespace
{
    Date make_day(const char *text) { return internal::parse_date(text); }

    Trade make_trade(const char *day, const char *ticker, double quantity)
    {
        return Trade{make_day(day), ticker, quantity};
    }

    // Open == close: Portfolio only uses closing prices.
    PriceBar make_bar(const char *day, const char *ticker, double close)
    {
        return PriceBar{make_day(day), ticker, close, close};
    }

    bool is_day(Date actual, const char *expected) { return actual == make_day(expected); }
}

TEST(PortfolioTest, BuyAndHoldProducesPriceReturns)
{
    // Buy 10 shares at the day-1 close; nothing else happens.
    //   value:   10*100 = 1000, 1100, 990, 1089
    //   returns: 1100/1000-1 = +0.10, 990/1100-1 = -0.10, 1089/990-1 = +0.10
    // No return on day 1: there was nothing held the day before.
    const std::vector<Trade> trades = {make_trade("2024-01-02", "AAA", 10)};
    const std::vector<PriceBar> prices = {
        make_bar("2024-01-02", "AAA", 100.0),
        make_bar("2024-01-03", "AAA", 110.0),
        make_bar("2024-01-04", "AAA", 99.0),
        make_bar("2024-01-05", "AAA", 108.9),
    };

    Portfolio portfolio(trades, prices);

    const TimeSeries &value = portfolio.value();
    ASSERT_EQ(value.size(), 4u);
    EXPECT_NEAR(value[0].value, 1000.0, 1e-9);
    EXPECT_NEAR(value[1].value, 1100.0, 1e-9);
    EXPECT_NEAR(value[2].value, 990.0, 1e-9);
    EXPECT_NEAR(value[3].value, 1089.0, 1e-9);

    const TimeSeries &returns = portfolio.returns();
    ASSERT_EQ(returns.size(), 3u);
    EXPECT_TRUE(is_day(returns[0].date, "2024-01-03"));
    EXPECT_NEAR(returns[0].value, 0.10, 1e-9);
    EXPECT_NEAR(returns[1].value, -0.10, 1e-9);
    EXPECT_NEAR(returns[2].value, 0.10, 1e-9);
}

TEST(PortfolioTest, BuyingMoreDoesNotDistortReturn)
{
    // The time-weighted-return example: +10% on 1000, then a 9900 deposit (buy 90 more
    // shares at the day-2 close), then another +10%.
    //   value:   1000, 100*110 = 11000, 100*121 = 12100
    //   returns: day 2 = 1100/1000-1 = 0.10   (the 9900 deposit is NOT counted as gain)
    //            day 3 = 12100/11000-1 = 0.10
    // A naive V_t/V_{t-1}-1 would report 1000% on day 2.
    const std::vector<Trade> trades = {
        make_trade("2024-01-02", "AAA", 10),
        make_trade("2024-01-03", "AAA", 90),
    };
    const std::vector<PriceBar> prices = {
        make_bar("2024-01-02", "AAA", 100.0),
        make_bar("2024-01-03", "AAA", 110.0),
        make_bar("2024-01-04", "AAA", 121.0),
    };

    Portfolio portfolio(trades, prices);

    const TimeSeries &value = portfolio.value();
    ASSERT_EQ(value.size(), 3u);
    EXPECT_NEAR(value[1].value, 11000.0, 1e-9);
    EXPECT_NEAR(value[2].value, 12100.0, 1e-9);

    const TimeSeries &returns = portfolio.returns();
    ASSERT_EQ(returns.size(), 2u);
    EXPECT_NEAR(returns[0].value, 0.10, 1e-9);
    EXPECT_NEAR(returns[1].value, 0.10, 1e-9);
    // Chain-linked: 1.10 * 1.10 - 1 = 21%.
    EXPECT_NEAR((1.0 + returns[0].value) * (1.0 + returns[1].value) - 1.0, 0.21, 1e-9);
}

TEST(PortfolioTest, SellingIsAWithdrawalNotALoss)
{
    // Buy 10 at 100; price flat on day 2 while 4 shares are sold; then +20% on day 3.
    //   value:   1000, 6*100 = 600, 6*120 = 720
    //   returns: day 2 = 1000/1000-1 = 0   (a naive value ratio would show -40%)
    //            day 3 = 720/600-1 = 0.20
    const std::vector<Trade> trades = {
        make_trade("2024-01-02", "AAA", 10),
        make_trade("2024-01-03", "AAA", -4),
    };
    const std::vector<PriceBar> prices = {
        make_bar("2024-01-02", "AAA", 100.0),
        make_bar("2024-01-03", "AAA", 100.0),
        make_bar("2024-01-04", "AAA", 120.0),
    };

    Portfolio portfolio(trades, prices);

    const TimeSeries &value = portfolio.value();
    ASSERT_EQ(value.size(), 3u);
    EXPECT_NEAR(value[1].value, 600.0, 1e-9);
    EXPECT_NEAR(value[2].value, 720.0, 1e-9);

    const TimeSeries &returns = portfolio.returns();
    ASSERT_EQ(returns.size(), 2u);
    EXPECT_NEAR(returns[0].value, 0.0, 1e-9);
    EXPECT_NEAR(returns[1].value, 0.20, 1e-9);
}

TEST(PortfolioTest, TradeOrderInInputDoesNotMatter)
{
    // Same data as BuyingMoreDoesNotDistortReturn, with the trades listed latest-first.
    const std::vector<Trade> trades = {
        make_trade("2024-01-03", "AAA", 90),
        make_trade("2024-01-02", "AAA", 10),
    };
    const std::vector<PriceBar> prices = {
        make_bar("2024-01-02", "AAA", 100.0),
        make_bar("2024-01-03", "AAA", 110.0),
        make_bar("2024-01-04", "AAA", 121.0),
    };

    Portfolio portfolio(trades, prices);

    const TimeSeries &returns = portfolio.returns();
    ASSERT_EQ(returns.size(), 2u);
    EXPECT_NEAR(returns[0].value, 0.10, 1e-9);
    EXPECT_NEAR(returns[1].value, 0.10, 1e-9);
}

TEST(PortfolioTest, TwoTickersAreValuedTogether)
{
    // A: 100, 110, 121 (hold 10)    B: 50, 50, 40 (hold 20), both bought on day 1.
    //   value:   10*100 + 20*50 = 2000, 1100 + 1000 = 2100, 1210 + 800 = 2010
    //   returns: 2100/2000-1 = 0.05, 2010/2100-1 = -0.042857142857...
    const std::vector<Trade> trades = {
        make_trade("2024-01-02", "A", 10),
        make_trade("2024-01-02", "B", 20),
    };
    const std::vector<PriceBar> prices = {
        make_bar("2024-01-02", "A", 100.0),
        make_bar("2024-01-02", "B", 50.0),
        make_bar("2024-01-03", "A", 110.0),
        make_bar("2024-01-03", "B", 50.0),
        make_bar("2024-01-04", "A", 121.0),
        make_bar("2024-01-04", "B", 40.0),
    };

    Portfolio portfolio(trades, prices);

    const TimeSeries &value = portfolio.value();
    ASSERT_EQ(value.size(), 3u);
    EXPECT_NEAR(value[0].value, 2000.0, 1e-9);
    EXPECT_NEAR(value[1].value, 2100.0, 1e-9);
    EXPECT_NEAR(value[2].value, 2010.0, 1e-9);

    const TimeSeries &returns = portfolio.returns();
    ASSERT_EQ(returns.size(), 2u);
    EXPECT_NEAR(returns[0].value, 0.05, 1e-9);
    EXPECT_NEAR(returns[1].value, 2010.0 / 2100.0 - 1.0, 1e-9);
}

TEST(PortfolioTest, FullLiquidationLeavesNoReturnsWhileFlat)
{
    // Buy 10, sell all 10 on day 2, then the price keeps moving while nothing is held.
    //   value:   1000, 0, 0, 0
    //   returns: only day 2 (the day the position was still held at the close): 0.0.
    //            Days 3 and 4 have V_{t-1} == 0, so no return exists.
    const std::vector<Trade> trades = {
        make_trade("2024-01-02", "AAA", 10),
        make_trade("2024-01-03", "AAA", -10),
    };
    const std::vector<PriceBar> prices = {
        make_bar("2024-01-02", "AAA", 100.0),
        make_bar("2024-01-03", "AAA", 100.0),
        make_bar("2024-01-04", "AAA", 120.0),
        make_bar("2024-01-05", "AAA", 130.0),
    };

    Portfolio portfolio(trades, prices);

    const TimeSeries &value = portfolio.value();
    ASSERT_EQ(value.size(), 4u);
    EXPECT_NEAR(value[0].value, 1000.0, 1e-9);
    EXPECT_NEAR(value[1].value, 0.0, 1e-9);
    EXPECT_NEAR(value[3].value, 0.0, 1e-9);

    const TimeSeries &returns = portfolio.returns();
    ASSERT_EQ(returns.size(), 1u);
    EXPECT_TRUE(is_day(returns[0].date, "2024-01-03"));
    EXPECT_NEAR(returns[0].value, 0.0, 1e-9);
}

TEST(PortfolioTest, MissingBarCarriesLastCloseForward)
{
    // B has no bar on day 2, so its day-1 close (50) is carried forward.
    //   value:   10*100 + 10*50 = 1500, 1500, 10*100 + 10*55 = 1550
    //   returns: 0.0, 1550/1500-1 = 0.033333...
    const std::vector<Trade> trades = {
        make_trade("2024-01-02", "A", 10),
        make_trade("2024-01-02", "B", 10),
    };
    const std::vector<PriceBar> prices = {
        make_bar("2024-01-02", "A", 100.0),
        make_bar("2024-01-02", "B", 50.0),
        make_bar("2024-01-03", "A", 100.0),
        make_bar("2024-01-04", "A", 100.0),
        make_bar("2024-01-04", "B", 55.0),
    };

    Portfolio portfolio(trades, prices);

    const TimeSeries &value = portfolio.value();
    ASSERT_EQ(value.size(), 3u);
    EXPECT_NEAR(value[1].value, 1500.0, 1e-9);
    EXPECT_NEAR(value[2].value, 1550.0, 1e-9);

    const TimeSeries &returns = portfolio.returns();
    ASSERT_EQ(returns.size(), 2u);
    EXPECT_NEAR(returns[0].value, 0.0, 1e-9);
    EXPECT_NEAR(returns[1].value, 1550.0 / 1500.0 - 1.0, 1e-9);
}

TEST(PortfolioTest, TradeOnNonTradingDayExecutesAtNextSession)
{
    // Calendar: Fri 01-05, Mon 01-08, Tue 01-09. The buy is dated Saturday 01-06, so it
    // executes at Monday's close (110).
    //   value:   starts Monday: 10*110 = 1100, 10*121 = 1210
    //   returns: Tuesday only: 1210/1100-1 = 0.10
    const std::vector<Trade> trades = {make_trade("2024-01-06", "AAA", 10)};
    const std::vector<PriceBar> prices = {
        make_bar("2024-01-05", "AAA", 100.0),
        make_bar("2024-01-08", "AAA", 110.0),
        make_bar("2024-01-09", "AAA", 121.0),
    };

    Portfolio portfolio(trades, prices);

    const TimeSeries &value = portfolio.value();
    ASSERT_EQ(value.size(), 2u);
    EXPECT_TRUE(is_day(value[0].date, "2024-01-08"));
    EXPECT_NEAR(value[0].value, 1100.0, 1e-9);
    EXPECT_NEAR(value[1].value, 1210.0, 1e-9);

    const TimeSeries &returns = portfolio.returns();
    ASSERT_EQ(returns.size(), 1u);
    EXPECT_TRUE(is_day(returns[0].date, "2024-01-09"));
    EXPECT_NEAR(returns[0].value, 0.10, 1e-9);
}

TEST(PortfolioTest, ThrowsWhenSellingMoreThanHeld)
{
    const std::vector<Trade> trades = {
        make_trade("2024-01-02", "AAA", 5),
        make_trade("2024-01-03", "AAA", -10),
    };
    const std::vector<PriceBar> prices = {
        make_bar("2024-01-02", "AAA", 100.0),
        make_bar("2024-01-03", "AAA", 100.0),
    };

    EXPECT_THROW(Portfolio portfolio(trades, prices), std::runtime_error);
}

TEST(PortfolioTest, ThrowsOnEmptyTrades)
{
    const std::vector<Trade> trades;
    const std::vector<PriceBar> prices = {make_bar("2024-01-02", "AAA", 100.0)};

    EXPECT_THROW(Portfolio portfolio(trades, prices), std::invalid_argument);
}

TEST(PortfolioTest, ThrowsWhenTickerHasNoPriceData)
{
    const std::vector<Trade> trades = {make_trade("2024-01-02", "AAA", 10)};
    const std::vector<PriceBar> prices = {make_bar("2024-01-02", "BBB", 100.0)};

    EXPECT_THROW(Portfolio portfolio(trades, prices), std::invalid_argument);
}

TEST(PortfolioTest, ThrowsOnDuplicatePriceBars)
{
    const std::vector<Trade> trades = {make_trade("2024-01-02", "AAA", 10)};
    const std::vector<PriceBar> prices = {
        make_bar("2024-01-02", "AAA", 100.0),
        make_bar("2024-01-02", "AAA", 101.0),
    };

    EXPECT_THROW(Portfolio portfolio(trades, prices), std::invalid_argument);
}

TEST(PortfolioTest, ThrowsWhenTradeIsAfterLastPriceDay)
{
    const std::vector<Trade> trades = {make_trade("2024-01-10", "AAA", 10)};
    const std::vector<PriceBar> prices = {
        make_bar("2024-01-02", "AAA", 100.0),
        make_bar("2024-01-03", "AAA", 101.0),
    };

    EXPECT_THROW(Portfolio portfolio(trades, prices), std::invalid_argument);
}

TEST(PortfolioTest, ThrowsWhenTradeIsBeforeFirstPriceDay)
{
    const std::vector<Trade> trades = {make_trade("2024-01-01", "AAA", 10)};
    const std::vector<PriceBar> prices = {
        make_bar("2024-01-02", "AAA", 100.0),
        make_bar("2024-01-03", "AAA", 101.0),
    };

    EXPECT_THROW(Portfolio portfolio(trades, prices), std::invalid_argument);
}

TEST(PortfolioTest, BenchmarkReturnsAlignToPortfolioReturnDates)
{
    // Portfolio returns exist on days 2 and 3 (same data as BuyingMoreDoesNotDistortReturn).
    // Benchmark closes 200, 210, 189 -> returns +0.05 and -0.10 on those same dates.
    const std::vector<Trade> trades = {
        make_trade("2024-01-02", "AAA", 10),
        make_trade("2024-01-03", "AAA", 90),
    };
    const std::vector<PriceBar> prices = {
        make_bar("2024-01-02", "AAA", 100.0),
        make_bar("2024-01-03", "AAA", 110.0),
        make_bar("2024-01-04", "AAA", 121.0),
    };
    const std::vector<PriceBar> benchmark = {
        make_bar("2024-01-02", "SPY", 200.0),
        make_bar("2024-01-03", "SPY", 210.0),
        make_bar("2024-01-04", "SPY", 189.0),
    };

    Portfolio portfolio(trades, prices);
    const TimeSeries benchmark_returns = portfolio.benchmark_returns(benchmark);

    ASSERT_EQ(benchmark_returns.size(), portfolio.returns().size());
    for (size_t i = 0; i < benchmark_returns.size(); ++i)
    {
        EXPECT_TRUE(benchmark_returns[i].date == portfolio.returns()[i].date);
    }
    EXPECT_NEAR(benchmark_returns[0].value, 0.05, 1e-9);
    EXPECT_NEAR(benchmark_returns[1].value, -0.10, 1e-9);
}

TEST(PortfolioTest, BenchmarkReturnUsesPreviousTradingDayAsBase)
{
    // The first buy is on day 2, so the only portfolio return is on day 3. The matching
    // benchmark return must be day 2 -> day 3 (198/220-1 = -0.10), not day 1 -> day 3.
    const std::vector<Trade> trades = {make_trade("2024-01-03", "AAA", 10)};
    const std::vector<PriceBar> prices = {
        make_bar("2024-01-02", "AAA", 100.0),
        make_bar("2024-01-03", "AAA", 100.0),
        make_bar("2024-01-04", "AAA", 110.0),
    };
    const std::vector<PriceBar> benchmark = {
        make_bar("2024-01-02", "SPY", 200.0),
        make_bar("2024-01-03", "SPY", 220.0),
        make_bar("2024-01-04", "SPY", 198.0),
    };

    Portfolio portfolio(trades, prices);
    const TimeSeries benchmark_returns = portfolio.benchmark_returns(benchmark);

    ASSERT_EQ(benchmark_returns.size(), 1u);
    EXPECT_TRUE(is_day(benchmark_returns[0].date, "2024-01-04"));
    EXPECT_NEAR(benchmark_returns[0].value, -0.10, 1e-9);
}

TEST(PortfolioTest, BenchmarkWithMultipleTickersThrows)
{
    const std::vector<Trade> trades = {make_trade("2024-01-02", "AAA", 10)};
    const std::vector<PriceBar> prices = {
        make_bar("2024-01-02", "AAA", 100.0),
        make_bar("2024-01-03", "AAA", 110.0),
    };
    const std::vector<PriceBar> benchmark = {
        make_bar("2024-01-02", "SPY", 200.0),
        make_bar("2024-01-03", "QQQ", 210.0),
    };

    Portfolio portfolio(trades, prices);
    EXPECT_THROW(portfolio.benchmark_returns(benchmark), std::invalid_argument);
}

TEST(PortfolioTest, BenchmarkStartingTooLateThrows)
{
    // Portfolio returns start on day 2, but the benchmark has no bar until day 3, so
    // there is no base price for the day-2 return.
    const std::vector<Trade> trades = {make_trade("2024-01-02", "AAA", 10)};
    const std::vector<PriceBar> prices = {
        make_bar("2024-01-02", "AAA", 100.0),
        make_bar("2024-01-03", "AAA", 110.0),
        make_bar("2024-01-04", "AAA", 121.0),
    };
    const std::vector<PriceBar> benchmark = {make_bar("2024-01-04", "SPY", 200.0)};

    Portfolio portfolio(trades, prices);
    EXPECT_THROW(portfolio.benchmark_returns(benchmark), std::runtime_error);
}