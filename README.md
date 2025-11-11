# UPDATE: I abandoned this project.

# Creating a Grid/Martingale Expert Advisor for MT5 to explore this world

The idea is to create a modular Expert Advisor for MetaTrader 5 designed around zone trading using Fibonacci levels derived from a higher timeframe “base period.” It implements a direction-aware grid with optional martingale sizing, monetary or pips-based take profit aggregation, session filters (days/hours), daily profit target shutdown, and a future-ready recovery mode.

## Disclaimer

Trading leveraged products involves significant risk. Do not play with real money using this...

## Key Features

- Modular architecture with clear, independent modules:
  - ControlSesion: day/time filters for trading windows
  - GestionOrdenes: order/grid management and risk controls
  - LogicaEntrada: orchestration of entries based on zones
  - CriteriosFinalizacion: daily target and day reset management
  - ModoRecuperacion: reserved for future recovery logic
- Zone-based trading using Fibonacci zones from a selected base timeframe (H4/Daily/Weekly/Monthly)
- Directional bias derived from previous base period candle
- Take Profit in Dollars or in Pips, aggregated at basket level by weighted average price
- Grid expansion by configurable dollar step; optional martingale from a given level
- Daily profit target (% of account balance) to stop new entries and switch to “Close-Only” mode
- Visual chart elements:
  - Dynamic Fibonacci object over the base period
  - Highlighted operative zones: Buy, Sell, Ranging, Out-of-Range

## How It Works

1. Base Period Analysis
   - On each tick, the EA checks if a new base period segment has formed (H4/Daily/Weekly/Monthly).
   - It computes previous period open/close/high/low using granular data (H1 or D1) and sets a directional bias:
     - If close > open: BUY_ONLY
     - If close < open: SELL_ONLY
     - If equal: NONE
   - It calculates Fibonacci price levels based on period high/low and draws an MT5 Fibonacci object.

2. Zone Detection
   - Using the previous period’s high/low, it divides price into:
     - ZONE_BUY: above 78.6%
     - ZONE_SELL: below 23.6%
     - ZONE_RANGING: between 23.6% and 78.6%
     - ZONE_OUT_OF_RANGE: beyond the period high or below the period low
   - The active zone is highlighted with rectangles and logged when it changes.

3. Session Control and Daily Target
   - Day-of-week and UTC time windows can be enforced. If outside, EA switches to Close-Only mode.
   - A daily profit target (% of starting day balance) can halt new entries and switch to Close-Only mode once reached.
   - Daily counters reset automatically when a new day starts (server time).

4. Order Management (Grid + Optional Martingale)
   - One basket per direction per symbol/magic.
   - If no position exists and not in Close-Only, EA opens an initial trade with TP computed either in dollars or pips.
   - If positions exist:
     - If price moves against the last entry by a configurable dollar step, EA can add a new grid order.
     - If martingale is enabled, lot size multiplies from a specified grid level.
   - TP is continuously recalculated to a basket-level target using the volume-weighted average entry price.
   - In ZONE_BUY/ZONE_SELL, it closes opposite direction baskets and manages the aligned side. In RANGING, it manages both sides. Out-of-Range forces Close-Only behavior.

5. Recovery Mode (Future)
   - Reserved for future implementation of recovery/hedging logic.
   - When enabled, it takes full control and displays a chart comment as placeholder.

## Inputs and Parameters

Identification
- _MagicNumber (ulong): Unique ID for this EA’s orders. Default: 1666

Base Timeframe and Fibonacci
- _BaseTimeframe (ENUM_BASE_TIMEFRAME): H4, DAILY, WEEKLY, MONTHLY. Default: H4
- _FibonacciLevelsStr (string): Comma-separated fib percentages. Default: "0.0,0.236,0.382,0.5,0.618,0.786,1.0"

Session Control (Módulo 1)
- _EnableDayFilter (bool): Enable day-of-week filter. Default: true
- _TradeOnMonday ... _TradeOnFriday (bool): Which weekdays to trade. Default Friday: false
- _EnableTimeFilter (bool): Enable UTC time filter. Default: true
- _StartTimeUTC (int): Start hour (UTC). Default: 8
- _EndTimeUTC (int): End hour (UTC, exclusive). Default: 11

Order and Risk Management (Módulo 3)
- _InitialLotSize (double): Initial lot size. Default: 0.01
- _TP_Mode (ENUM_TP_MODE): IN_DOLLARS or IN_PIPS. Default: IN_DOLLARS
- _TakeProfitValue (double): If IN_DOLLARS, monetary TP per basket; if IN_PIPS, pips TP. Default: 0.5
- _PipStepValue (double): Distance in dollars used to trigger new grid entries against price. Default: 5.0
- _EnableMartingale (bool): Enable martingale sizing. Default: true
- _MartingaleMultiplier (double): Lot multiplier for martingale. Default: 2.0
- _MartingaleStartLevel (int): Grid index at which martingale starts (2 means at 3rd order). Default: 2

Finish Criteria (Módulo 4)
- _DailyProfitTarget (double): Daily profit target as % of starting day balance. Default: 1.0

Recovery Mode (Módulo 5)
- _EnableRecoveryMode (bool): Future feature toggle. Default: false

Notes on monetary TP
- In IN_DOLLARS mode, the EA estimates required points based on TickValue/TickSize/Point of the symbol, then converts desired $ target into a TP price for the basket (weighted average).
- In IN_PIPS mode, it uses a fixed pips distance.

---------------------------------------------------------------------

## Visual Elements

- Fibonacci object “FiboRetracement_<BaseTimeframe>” drawn across the previous base period
- Zone rectangles:
  - Zone_Upper_OUT / Zone_Lower_OUT (Out-of-Range: dark red tone)
  - Zone_Buy / Zone_Sell (buy/sell zones: yellow tone)
  - Zone_Ranging (green tone)
- Chart shift is enabled for clarity

---------------------------------------------------------------------

## File Structure

- ElProfeXau_EA.mq5: Main Expert Advisor
- ControlSesion.mqh: Session filters and day/time logic
- CriteriosFinalizacion.mqh: Daily target and day reset logic
- GestionOrdenes.mqh: Order, grid, TP recalculation, and martingale logic
- LogicaEntrada.mqh: Entry orchestration per operative zone
- ModoRecuperacion.mqh: Placeholder for future recovery logic

Additionally uses:
- <Trade/Trade.mqh>, <Trade/SymbolInfo.mqh> (MT5 standard)
- <Arrays/ArrayObj.mqh> (MT5 standard)

---------------------------------------------------------------------

## Installation

1. Open MetaTrader 5.
2. File > Open Data Folder.
3. Place:
   - ElProfeXau_EA.mq5 into MQL5/Experts/
   - The .mqh modules into MQL5/Experts/Include/ or MQL5/Include/ (keep paths consistent with your includes).
4. Ensure MT5 standard libraries are available (<Trade>, <Arrays>, etc.).
5. Recompile the EA in MetaEditor.
6. Attach the EA to your desired chart and set inputs.

---------------------------------------------------------------------

## Usage Tips

- Timezone: Session filtering uses server time converted to UTC via TimeCurrent. Verify your broker’s server time vs your intended trading window.
- Symbols: Each chart/symbol runs its own instance. Use distinct magic numbers to avoid cross-interference if running multiple instances or strategies.
- Friday Trading: Off by default. Enable if required.
- Base Timeframe: Choose based on your style:
  - H4/Daily for more frequent directional updates
  - Weekly/Monthly for broader trend bias and wider zones
- Monetary TP Calibration: Different symbols have different TickSize/Value dynamics. Validate TP distances on your broker’s contract spec before going live.
- Grid Spacing: _PipStepValue is specified in dollars of price distance. Make sure it aligns with the instrument’s price scale and volatility.
- Martingale: Powerful but risky. Consider setting a max lot cap (future enhancement) and test thoroughly.

---------------------------------------------------------------------

## Safety and Risk

- This EA can open multiple positions per direction as price moves against the last grid entry.
- Martingale increases lot size, which can grow exposure rapidly in adverse trends.
- Always test on demo first, then use small risk in live.
- Consider adding broker-side protections:
  - Max lot limits
  - Max spread/volatility filters
  - Equity drawdown protections
  - News filters
- Daily target helps lock daily gains by switching to Close-Only, but does not prevent intraday drawdown.

---------------------------------------------------------------------

## Logging

- The EA prints key events:
  - New base period detections
  - Direction changes
  - Zone transitions
  - Transition to Close-Only when daily target is reached
- Use the MT5 Experts/Journal tabs to monitor.


## Quick Start

- Keep default Base Timeframe: H4; Fibonacci levels as provided.
- Enable only Monday–Thursday, UTC 8–11 for a conservative session window.
- Start with:
  - _InitialLotSize: 0.01
  - _TP_Mode: IN_DOLLARS
  - _TakeProfitValue: 0.5
  - _PipStepValue: 5.0
  - _EnableMartingale: off (for first tests)
- Run on demo for several days; review logs and behavior.
- Adjust parameters based on your risk tolerance and instrument specifics.
