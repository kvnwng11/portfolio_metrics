#include <filesystem>
#include <fstream>
#include <string>

#include <gtest/gtest.h>

#include "datalayer.h"

namespace
{
    // Writes a CSV to the temp directory and deletes it when the test ends. Written in
    // binary mode so explicit "\r\n" in the contents is preserved exactly.
    class TempCsv
    {
    public:
        TempCsv(const std::string &name, const std::string &contents)
            : path_(std::filesystem::temp_directory_path() / ("portfolio_metrics_test_" + name))
        {
            std::ofstream out(path_, std::ios::binary);
            out << contents;
        }
        TempCsv(const TempCsv &) = delete;
        TempCsv &operator=(const TempCsv &) = delete;
        ~TempCsv()
        {
            std::error_code ec;
            std::filesystem::remove(path_, ec);
        }
        const std::filesystem::path &path() const { return path_; }

    private:
        std::filesystem::path path_;
    };

    bool is_day(Date actual, const char *expected) { return actual == internal::parse_date(expected); }

    bool contains(const std::string &text, const std::string &fragment)
    {
        return text.find(fragment) != std::string::npos;
    }
}

TEST(DataLayerTest, LoadsTrades)
{
    TempCsv file("trades_basic.csv",
                 "day,ticker,quantity\n"
                 "2024-01-02,AAPL,10\n"
                 "2024-01-03,AAPL,-5.5\n");

    const std::vector<Trade> trades = datalayer::load_trades(file.path());

    ASSERT_EQ(trades.size(), 2u);
    EXPECT_TRUE(is_day(trades[0].day, "2024-01-02"));
    EXPECT_EQ(trades[0].ticker, "AAPL");
    EXPECT_DOUBLE_EQ(trades[0].quantity, 10.0);
    EXPECT_TRUE(is_day(trades[1].day, "2024-01-03"));
    EXPECT_DOUBLE_EQ(trades[1].quantity, -5.5);
}

TEST(DataLayerTest, ToleratesBomCrlfBlankLinesColumnOrderAndExtraColumns)
{
    const std::string bom = "\xEF\xBB\xBF";
    TempCsv file("trades_messy.csv",
                 bom + "Ticker,Extra,Quantity,Day\r\n" +
                     "\r\n" +
                     "MSFT,ignored,3,2024-02-01\r\n" +
                     "\r\n");

    const std::vector<Trade> trades = datalayer::load_trades(file.path());

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].ticker, "MSFT");
    EXPECT_DOUBLE_EQ(trades[0].quantity, 3.0);
    EXPECT_TRUE(is_day(trades[0].day, "2024-02-01"));
}

TEST(DataLayerTest, LoadsPricesInScraperFormat)
{
    TempCsv file("prices_basic.csv",
                 "day,ticker,open_price,close_price\n"
                 "2024-01-02,AAPL,185.5,186.25\n"
                 "2024-01-02 00:00:00,^GSPC,4700.5,4742.75\n");

    const std::vector<PriceBar> bars = datalayer::load_prices(file.path());

    ASSERT_EQ(bars.size(), 2u);
    EXPECT_EQ(bars[0].ticker, "AAPL");
    EXPECT_DOUBLE_EQ(bars[0].open_price, 185.5);
    EXPECT_DOUBLE_EQ(bars[0].close_price, 186.25);
    EXPECT_EQ(bars[1].ticker, "^GSPC");
    EXPECT_TRUE(is_day(bars[1].day, "2024-01-02")); // time-of-day suffix is accepted
    EXPECT_DOUBLE_EQ(bars[1].close_price, 4742.75);
}

TEST(DataLayerTest, ThrowsOnMissingFile)
{
    EXPECT_THROW(datalayer::load_trades("this_file_does_not_exist.csv"), std::runtime_error);
}

TEST(DataLayerTest, ThrowsOnEmptyFile)
{
    TempCsv file("empty.csv", "");
    EXPECT_THROW(datalayer::load_trades(file.path()), std::runtime_error);
}

TEST(DataLayerTest, ThrowsOnMissingColumn)
{
    TempCsv file("trades_no_quantity.csv",
                 "day,ticker\n"
                 "2024-01-02,AAPL\n");

    try
    {
        datalayer::load_trades(file.path());
        FAIL() << "expected an exception";
    }
    catch (const std::runtime_error &error)
    {
        EXPECT_TRUE(contains(error.what(), "quantity"));
    }
}

TEST(DataLayerTest, ThrowsOnInvalidDateAndNamesTheLine)
{
    // 2026 is not a leap year, so 2026-02-29 doesn't exist. It's on line 3.
    TempCsv file("trades_bad_date.csv",
                 "day,ticker,quantity\n"
                 "2024-01-02,AAPL,10\n"
                 "2026-02-29,AAPL,5\n");

    try
    {
        datalayer::load_trades(file.path());
        FAIL() << "expected an exception";
    }
    catch (const std::runtime_error &error)
    {
        EXPECT_TRUE(contains(error.what(), ":3:"));
        EXPECT_TRUE(contains(error.what(), "2026-02-29"));
    }
}

TEST(DataLayerTest, ThrowsOnTrailingGarbageAfterDate)
{
    TempCsv file("trades_date_garbage.csv",
                 "day,ticker,quantity\n"
                 "2024-01-02garbage,AAPL,10\n");

    EXPECT_THROW(datalayer::load_trades(file.path()), std::runtime_error);
}

TEST(DataLayerTest, ThrowsOnInvalidNumber)
{
    TempCsv file("trades_bad_number.csv",
                 "day,ticker,quantity\n"
                 "2024-01-02,AAPL,ten\n");

    EXPECT_THROW(datalayer::load_trades(file.path()), std::runtime_error);
}

TEST(DataLayerTest, ThrowsOnRowWithTooFewColumns)
{
    TempCsv file("trades_short_row.csv",
                 "day,ticker,quantity\n"
                 "2024-01-02,AAPL\n");

    EXPECT_THROW(datalayer::load_trades(file.path()), std::runtime_error);
}

TEST(DataLayerTest, ThrowsOnNonPositivePrice)
{
    TempCsv file("prices_zero.csv",
                 "day,ticker,open_price,close_price\n"
                 "2024-01-02,AAPL,185.5,0\n");

    EXPECT_THROW(datalayer::load_prices(file.path()), std::runtime_error);
}

TEST(DataLayerTest, ThrowsOnNanPrice)
{
    TempCsv file("prices_nan.csv",
                 "day,ticker,open_price,close_price\n"
                 "2024-01-02,AAPL,nan,186.25\n");

    EXPECT_THROW(datalayer::load_prices(file.path()), std::runtime_error);
}