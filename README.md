# Event-Driven Market-Making Simulator with Inventory Risk Management

A high-performance C++ market-making simulator that replays historical tick data through an event-driven pipeline, runs an Avellaneda–Stoikov optimal quoting strategy with live volatility estimation and inventory risk controls, and visualizes the full system state through a real-time WebSocket-connected dashboard.

## What This Does

Market makers provide liquidity by continuously quoting bid and ask prices on an asset. The core challenge is inventory risk — when your fills are imbalanced, you accumulate a position that exposes you to adverse price moves. This simulator implements the Avellaneda–Stoikov (2008) framework, which derives mathematically optimal quotes that balance spread revenue against inventory risk in real time.

The system replays tick data from CSV through a lock-free event pipeline, computes optimal bid/ask placement using live volatility estimates, executes trades through a price-time priority matching engine, and streams the full state to a browser dashboard at 10Hz.

## Architecture

```
 Tick Data (CSV)
      │
      ▼
┌──────────────┐     ┌───────────────────────────┐
│  Market Data  │────▶│  SPSC Lock-Free Ring Buf  │
│  Feed Replay  │     │  (Event Bus)              │
└──────────────┘     └─────────┬─────────────────┘
                               │
                    ┌──────────▼──────────┐
                    │  Avellaneda-Stoikov  │◀── Volatility Estimator
                    │  Strategy Engine     │    (CC / Parkinson / YZ)
                    └──────────┬──────────┘
                               │
                    ┌──────────▼──────────┐
                    │  Order Manager       │
                    │  + Matching Engine   │
                    └──────────┬──────────┘
                               │
              ┌────────────────┼────────────────┐
              ▼                ▼                 ▼
     ┌──────────────┐  ┌────────────┐  ┌──────────────┐
     │ Limit Order   │  │   Risk     │  │  WebSocket   │
     │ Book (LOB)    │  │  Manager   │  │  Server      │
     └──────────────┘  │ + Analytics│  └──────┬───────┘
                       └────────────┘         │
                                              ▼
                                     ┌──────────────┐
                                     │    React      │
                                     │   Dashboard   │
                                     └──────────────┘
```

Event-driven, single-threaded hot path. Every tick flows through the pipeline as a typed event on a lock-free SPSC ring buffer. The strategy recalculates optimal quotes, the order manager updates the book, and the matching engine resolves fills — with heap allocation avoided in the critical path where possible.

## Key Results

### Strategy comparison: Avellaneda–Stoikov vs naive fixed spread

Offline **A/B** runs replay the **same CSV** twice (parameters from the live server config). Metrics are **path-dependent** (fills differ); treat numbers as **dataset-specific** — generate your own table from the dashboard’s **Strategy comparison** panel after a run.

The Avellaneda–Stoikov strategy uses a **reservation price** that shifts when inventory builds, skewing quotes toward reducing position — a mechanism absent in naive fixed-spread quoting.

### Volatility estimator convergence

Synthetic GBM tests and manual checks (`tests/test_volatility_estimators_manual.cpp`) compare close-to-close, Parkinson, and Yang–Zhang estimators against a known σ. Yang–Zhang typically uses OHLC information efficiently; the **default** choice for live σ depends on your data and window — run the manual test or your own calibration to compare.

## Performance

Microbenchmarks are built as **`mmsim_bench`** (Google Benchmark). Example run on a **32-thread @ ~2.4 GHz** Windows machine (numbers vary by CPU and load):

| Benchmark | Time (ns/op) | Notes |
|-----------|--------------|--------|
| `event_bus/push_pop_pair` | ~6 | SPSC ring: one push + one pop |
| `order_book/add_cancel/8` | ~780 | Add + cancel; book depth 8/side |
| `order_book/add_cancel/64` | ~4.2k | Deeper book |
| `matching_engine/aggressive_buy_match` | ~5 | Hot-path aggressive match |
| `pipeline/tick_to_trade` | ~28 | Bus pop + `risk.mark` + AS quotes + one match |

**Run locally:**

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DMMSIM_BUILD_BENCH=ON
cmake --build build --target mmsim_bench
./build/bench/mmsim_bench    # or build/bench/mmsim_bench.exe on Windows
```

CSV export: `./build/bench/mmsim_bench --benchmark_format=csv`.

For profiling, use **perf** (Linux), **Very Sleepy**, Visual Studio **CPU Usage**, or **ETW** on Windows against `mmsim_ws_server` (symbols required).

## Dashboard

The React dashboard connects over WebSocket and renders the simulation state in real time:

- **Order book depth** — bid/ask volume bars with the strategy’s quotes in context
- **PnL curve** — realized and unrealized PnL
- **Inventory** — current position vs limits
- **Volatility panel** — multiple estimators side by side
- **Trade log** — fills with timestamps, side, price, and inventory
- **Parameter controls** — γ, κ, vol window, and risk settings while the simulation runs


![Alt text](/images/one.png)

### Depth and Cumulative PNL
![Alt text](/images/four.png)

### Strategy Comparision after running the A/B test
![Alt text](/images/two.png)

### Inventory and Volatility
![Alt text](/images/three.png)



## Quick start

### Prerequisites

- **CMake** ≥ 3.20, **C++20** compiler (GCC 12+, Clang 15+, or MSVC)
- **Node.js** 18+ (for the dashboard)
- **Git** (FetchContent pulls GoogleTest, Benchmark, websocketpp, nlohmann/json, Asio)

### Build (C++)

```bash
git clone https://github.com/yourusername/cpp-market-making-simulator.git
cd cpp-market-making-simulator

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Artifacts (paths may vary):

| Target | Description |
|--------|-------------|
| `mmsim_core` | Static library |
| `mmsim_server` | Batch CSV replay (CLI) |
| `mmsim_ws_server` | WebSocket server + simulation |
| `mmsim_tests` | Unit tests |
| `mmsim_bench` | Microbenchmarks (optional) |

### Run batch simulation

```bash
./build/server/mmsim_server --data data/sample_ticks.csv --gamma 0.5 --kappa 1.0
```

### Run WebSocket server + dashboard

```bash
# Terminal 1 — from repo root so default data path resolves
./build/server/mmsim_ws_server --data data/sample_ticks.csv --port 8080

# Terminal 2
cd dashboard
npm install
npm run dev
```

Open the printed local URL (e.g. `http://localhost:5173`). The UI expects `ws://localhost:8080` unless you change it in `App.tsx`.

### Tests

```bash
cd build && ctest --output-on-failure
```

### Benchmarks

```bash
cd build && ./bench/mmsim_bench
```

## Design decisions

**Why a lock-free SPSC ring buffer?** In the hot path, mutex contention adds unpredictable latency. The SPSC ring buffer gives bounded, deterministic behavior for push/pop — one producer (feed) and one consumer (sim loop) match this linear pipeline.

**Why `std::map` for price levels?** Real data spans an unpredictable price range. A flat array would require fixed bounds and waste memory on sparse regions. `std::map` yields O(log N) per level with N = active price levels (typically modest).

**Why three volatility estimators?** Close-to-close is standard; Parkinson uses range information; Yang–Zhang adds open/close handling and is useful for OHLC-style bars. Running all three exposes how estimator choice affects strategy behavior.

**Fixed-point prices** — `Price` / `Quantity` as integers (`Price` × 10 000) avoids floating-point ordering bugs in the book and keeps PnL arithmetic deterministic; conversion to display happens at the serialization boundary.

## Project structure

```
cpp-market-making-simulator/
├── .github/workflows/     CI
├── core/                  mmsim_core — engine, strategies, risk, volatility
├── server/                mmsim_server, mmsim_ws_server, comparison_sim, WebSocket hub
├── dashboard/             React + TypeScript + Vite + Recharts
├── bench/                 Google Benchmark microbenchmarks
├── tests/                 GoogleTest suite
├── data/                  sample CSV ticks
├── LICENSE
└── .clang-format
```

## References

- Avellaneda, M. & Stoikov, S. (2008). High-frequency trading in a limit order book. *Quantitative Finance*, 8(3), 217–224.
- Yang, D. & Zhang, Q. (2000). Drift-independent volatility estimation based on high, low, open, and close prices. *Journal of Business*, 73(3), 477–492.
- Parkinson, M. (1980). The extreme value method for estimating the variance of the rate of return. *Journal of Business*, 53(1), 61–65.
- Thompson, T. (2011). LMAX Disruptor: High performance alternative to bounded queues. LMAX Exchange.

## License

This project is licensed under the [MIT License](LICENSE).
