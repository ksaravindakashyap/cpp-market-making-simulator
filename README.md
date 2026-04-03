# cpp-market-making-simulator

A **C++20 market-making simulator** with an **Avellaneda–Stoikov** quoting model, a **naive fixed-spread** baseline, **limit-order-book** matching, **risk / PnL analytics**, and a **React** dashboard (**QuantFlow**) that connects over **WebSockets**. Use it to replay tick CSVs, tune γ/κ and risk limits, and compare strategies on identical data.

---

## Features

- **Core engine**: `LimitOrderBook`, `MatchingEngine`, `OrderManager`, `RiskManager` (PnL, drawdown, Sharpe/Sortino, inventory stats).
- **Strategies**: Avellaneda–Stoikov optimal quotes vs. symmetric fixed half-spread (`fixed_spread_mm`).
- **Replay**: CSV market ticks → event bus → strategy → simulated liquidity taking.
- **Servers**: `mmsim_server` (CLI batch), `mmsim_ws_server` (live WebSocket + control channel + offline A/B comparison).
- **Dashboard**: real-time book, PnL chart, volatility estimators, trade log, control sliders, **strategy comparison report** (markdown + table).

---

## Key results (Avellaneda–Stoikov vs naive fixed spread)

Offline **A/B** runs replay the **same CSV** twice (parameters from the live server config). Metrics are **path-dependent** (fills differ); treat numbers as **dataset-specific**.

| Metric | Typical use | Avellaneda–Stoikov | Fixed spread (naive) |
|--------|-------------|--------------------|----------------------|
| Role | — | Inventory-aware reservation price + intensity-based spread | Constant half-spread around mid |
| Sharpe / Sortino | Risk-adjusted returns | Often better when inventory risk dominates | Baseline; can be competitive if spread matches microstructure |
| Max drawdown | Tail risk | Varies with γ, κ, σ, τ | Often simpler inventory dynamics |
| PnL & inventory stats | Outcome | From `RiskManager` after full replay | Same |

**How to generate your table:** start `mmsim_ws_server`, open the dashboard → **Control** → set **fixed half-spread** → **Run A/B comparison**. The **Strategy comparison** panel shows a side-by-side table and markdown export.

---

## Performance benchmarks

Built with `mmsim_bench` (Google Benchmark). Example run on a **32-thread @ ~2.4 GHz** Windows machine (numbers vary by CPU and load):

| Benchmark | Time (ns/op) | Notes |
|-----------|--------------|--------|
| `event_bus/push_pop_pair` | ~6 | SPSC ring: one push + one pop |
| `order_book/add_cancel/8` | ~780 | Add + cancel; book depth 8/side |
| `order_book/add_cancel/64` | ~4.2k | Deeper book |
| `order_book/add_cancel/512` | ~25k | |
| `order_book/add_cancel/4096` | ~250k–460k | Worst-case depth in range |
| `matching_engine/aggressive_buy_match` | ~5 | Hot-path aggressive match |
| `pipeline/tick_to_trade` | ~28 | Bus pop + `risk.mark` + AS quotes + one match |

**Run locally:**

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DMMSIM_BUILD_BENCH=ON
cmake --build build --target mmsim_bench
./build/bench/mmsim_bench    # or mmsim_bench.exe on Windows
```

CSV export: `./build/bench/mmsim_bench --benchmark_format=csv`.

---

## Quick start

### Prerequisites

- **CMake** ≥ 3.20, **C++20** compiler (MSVC, GCC, or Clang)
- **Node.js** 18+ (for the dashboard)
- **Git** (FetchContent pulls GoogleTest, Benchmark, websocketpp, nlohmann/json, Asio)

### Build (C++)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Artifacts (paths may vary):

| Target | Description |
|--------|-------------|
| `mmsim_core` | Static library |
| `mmsim_server` | Batch CSV replay (CLI) |
| `mmsim_ws_server` | WebSocket server + simulation |
| `mmsim_tests` | Unit tests (`ctest` or run executable) |
| `mmsim_bench` | Microbenchmarks (optional: `-DMMSIM_BUILD_BENCH=ON`) |

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

### Tests & benchmarks

```bash
cd build && ctest --output-on-failure
./bench/mmsim_bench
```

---

## Tech stack

| Layer | Technology |
|-------|------------|
| Language | C++20 |
| Build | CMake |
| Core tests | GoogleTest |
| Benchmarks | Google Benchmark |
| JSON | nlohmann/json |
| WebSocket server | websocketpp + Asio (standalone) |
| Frontend | TypeScript, React 19, Vite 6 |
| Styling | Tailwind CSS |
| Charts | Recharts |

---

## Repository layout

```
├── .github/workflows/    # GitHub Actions CI
├── core/                 # mmsim_core — engine + strategies + risk
├── server/               # mmsim_server, mmsim_ws_server, comparison_sim, ws hub
├── bench/                # mmsim_bench
├── tests/                # unit tests
├── dashboard/            # QuantFlow UI
├── data/                 # sample CSV ticks
├── LICENSE               # MIT
└── .clang-format         # C++ style (enforced in CI)
```

---

## License

This project is licensed under the [MIT License](LICENSE).

---

## CI & code style

GitHub Actions (`.github/workflows/ci.yml`) builds on **Ubuntu** with **g++**, runs **`ctest`**, runs **`mmsim_bench`** as a smoke test, and checks **`clang-format`** against `core/`, `server/`, `tests/`, and `bench/` using `.clang-format`.

Format locally (requires `clang-format` on your `PATH`, e.g. `pip install clang-format` on Windows):

```bash
find core server tests bench \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) -print0 | xargs -0 clang-format -i
```

PowerShell:

```powershell
Get-ChildItem core, server, tests, bench -Recurse -Include *.cpp, *.h, *.hpp | ForEach-Object { clang-format -i $_.FullName }
```

---

## Project status

Project structure and dependencies are defined in the root `CMakeLists.txt` and per-target `CMakeLists.txt`. Extend or pin versions there as needed for your environment.
