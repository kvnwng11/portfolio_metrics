"""
Given a csv of trade data, get the daily prices of all assets within the trading window.
Also outputs the daily prices of a benchmark over the trading period.

Outputs:
    1. A portfolio csv with schema (day, ticker, open price, close price)
    2. A benchmark csv with schema (day, ticker, open price, close price)

Usage:
    python scripts/price_scraper.py --trades data/sample_portfolio2/trades.csv --out-dir data/sample_portfolio2 --benchmark ^GSPC
"""

import argparse
import sys
from pathlib import Path

import pandas as pd
import yfinance as yf


def load_trades(trades_path: Path) -> pd.DataFrame:
    """Load trade data and validate required columns are present."""
    trades = pd.read_csv(trades_path)
    required_cols = {"day", "ticker", "quantity"}
    missing = required_cols - set(trades.columns)
    if missing:
        raise ValueError(f"trades csv is missing required columns: {missing}")

    # Parse dates explicitly (rather than via read_csv's parse_dates) so a bad
    # or inconsistently-formatted value raises a clear error instead of silently
    # leaving the column as strings, which fails later with a confusing TypeError.
    parsed = pd.to_datetime(trades["day"], errors="coerce")
    bad_rows = trades.loc[parsed.isna(), "day"]
    if not bad_rows.empty:
        examples = bad_rows.unique()[:5]
        raise ValueError(
            f"could not parse {len(bad_rows)} value(s) in 'day' column of {trades_path}; "
            f"examples: {list(examples)}. Expected a consistent date format, e.g. YYYY-MM-DD."
        )
    trades["day"] = parsed
    return trades


def get_trading_window(trades: pd.DataFrame, buffer_days: int = 5) -> tuple[pd.Timestamp, pd.Timestamp]:
    """Return (start, end) dates covering all trades, with a small buffer so the
    first trade's day has a preceding price point to compute a return against."""
    start = trades["day"].min() - pd.Timedelta(days=buffer_days)
    end = trades["day"].max()
    return start, end


def fetch_daily_prices(ticker: str, start: pd.Timestamp, end: pd.Timestamp) -> pd.DataFrame:
    """Fetch daily open/close prices for a single ticker over [start, end]."""
    # yfinance's `end` is exclusive, so push it out by one day to include the last trading day.
    data = yf.download(
        ticker,
        start=start.date(),
        end=(end + pd.Timedelta(days=1)).date(),
        interval="1d",
        auto_adjust=False,
        progress=False,
    )

    if data.empty:
        print(f"warning: no price data returned for {ticker}", file=sys.stderr)
        return pd.DataFrame(columns=["day", "ticker", "open_price", "close_price"])

    # yfinance can return a MultiIndex column set when auto_adjust/group_by defaults shift;
    # normalize to flat columns before selecting.
    if isinstance(data.columns, pd.MultiIndex):
        data.columns = data.columns.get_level_values(0)

    out = data.reset_index()[["Date", "Open", "Close"]].rename(
        columns={"Date": "day", "Open": "open_price", "Close": "close_price"}
    )
    out["ticker"] = ticker
    return out[["day", "ticker", "open_price", "close_price"]]


def fetch_all(tickers: list[str], start: pd.Timestamp, end: pd.Timestamp) -> pd.DataFrame:
    """Fetch daily prices for a list of tickers and concatenate into one long-format frame."""
    frames = []
    for ticker in tickers:
        print(f"fetching {ticker}...")
        frames.append(fetch_daily_prices(ticker, start, end))
    if not frames:
        return pd.DataFrame(columns=["day", "ticker", "open_price", "close_price"])
    return pd.concat(frames, ignore_index=True).sort_values(["day", "ticker"])


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--trades", type=Path, required=True, help="Path to trades csv (day,ticker,quantity)")
    parser.add_argument("--out-dir", type=Path, required=True, help="Directory to write prices.csv and benchmark.csv")
    parser.add_argument("--benchmark", type=str, default="^GSPC", help="Benchmark ticker symbol (default: ^GSPC)")
    parser.add_argument("--buffer-days", type=int, default=5, help="Extra calendar days to fetch before the first trade")
    args = parser.parse_args()

    args.out_dir.mkdir(parents=True, exist_ok=True)

    trades = load_trades(args.trades)
    tickers = sorted(trades["ticker"].unique())
    start, end = get_trading_window(trades, buffer_days=args.buffer_days)

    print(f"trading window: {start.date()} to {end.date()}")
    print(f"assets: {tickers}")

    portfolio_prices = fetch_all(tickers, start, end)
    portfolio_out = args.out_dir / "prices.csv"
    portfolio_prices.to_csv(portfolio_out, index=False)
    print(f"wrote {len(portfolio_prices)} rows to {portfolio_out}")

    benchmark_prices = fetch_all([args.benchmark], start, end)
    benchmark_out = args.out_dir / "benchmark.csv"
    benchmark_prices.to_csv(benchmark_out, index=False)
    print(f"wrote {len(benchmark_prices)} rows to {benchmark_out}")


if __name__ == "__main__":
    main()