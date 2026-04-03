#include "mmsim/avellaneda_stoikov.h"
#include "mmsim/event_bus.h"
#include "mmsim/event_types.h"
#include "mmsim/matching_engine.h"
#include "mmsim/order_book.h"
#include "mmsim/risk_manager.h"

#include <benchmark/benchmark.h>

#include <limits>
#include <vector>

// Google Benchmark prints a fixed-width console table (Benchmark | Time | CPU | Iterations | ...).
// CSV (spreadsheet-friendly):  mmsim_bench --benchmark_format=csv
// JSON:                         mmsim_bench --benchmark_format=json

namespace {

using mmsim::AvellanedaStoikovStrategy;
using mmsim::Event;
using mmsim::EventBus;
using mmsim::EventType;
using mmsim::FillData;
using mmsim::LimitOrderBook;
using mmsim::MarketTickData;
using mmsim::MatchingEngine;
using mmsim::OrderId;
using mmsim::Price;
using mmsim::Quantity;
using mmsim::RiskManager;
using mmsim::Side;

constexpr Quantity k_deep_liquidity =
    1'000'000'000'000LL; // avoid exhausting resting size during long runs

// --- Event bus: push + pop (same thread), throughput-oriented ---

static void BM_EventBus_PushPop_Throughput(benchmark::State& state) {
    EventBus bus(65536);
    Event e{};
    e.type = EventType::TimerTick;
    e.timer_tick.timestamp_ns = 1;

    for (auto _ : state) {
        static_cast<void>(bus.try_push(e));
        Event out{};
        static_cast<void>(bus.try_pop(out));
        benchmark::DoNotOptimize(out);
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * 2);
}
BENCHMARK(BM_EventBus_PushPop_Throughput)->Name("event_bus/push_pop_pair");

// --- Order book: add + cancel at various depths (both sides pre-seeded) ---

static void BM_OrderBook_AddCancel_Latency(benchmark::State& state) {
    const int depth = static_cast<int>(state.range(0));

    for (auto _ : state) {
        state.PauseTiming();
        LimitOrderBook book;
        OrderId next = 1;
        for (int i = 0; i < depth; ++i) {
            static_cast<void>(book.add_order(next++, Side::BUY, static_cast<Price>(10000 - i), 1));
            static_cast<void>(book.add_order(next++, Side::SELL, static_cast<Price>(20000 + i), 1));
        }
        state.ResumeTiming();

        const OrderId x = next++;
        static_cast<void>(book.add_order(x, Side::BUY, static_cast<Price>(5000), 1));
        static_cast<void>(book.cancel_order(x));
        benchmark::DoNotOptimize(book.order_count());
    }
}
BENCHMARK(BM_OrderBook_AddCancel_Latency)
    ->Name("order_book/add_cancel")
    ->Arg(8)
    ->Arg(64)
    ->Arg(512)
    ->Arg(4096)
    ->Unit(benchmark::kNanosecond);

// --- Matching engine: aggressive buys against resting asks ---

static void BM_MatchingEngine_SubmitThroughput(benchmark::State& state) {
    LimitOrderBook book;
    MatchingEngine engine(book);
    static_cast<void>(book.add_order(1, Side::SELL, 10100, k_deep_liquidity));

    std::vector<FillData> fills;
    fills.reserve(32);
    OrderId oid = 2;

    for (auto _ : state) {
        fills.clear();
        static_cast<void>(engine.submit_order(oid++, Side::BUY, std::numeric_limits<Price>::max(),
                                              Quantity{5}, fills));
        benchmark::DoNotOptimize(fills.size());
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MatchingEngine_SubmitThroughput)->Name("matching_engine/aggressive_buy_match");

// --- Full pipeline: tick event -> bus -> risk mark -> Avellaneda quotes -> match ---

static void BM_Pipeline_TickToTrade_Latency(benchmark::State& state) {
    EventBus bus(65536);
    mmsim::RiskManager::Config rcfg;
    rcfg.max_position = 0;
    RiskManager risk(rcfg);
    AvellanedaStoikovStrategy::Config scfg;
    scfg.gamma = 0.5;
    scfg.kappa = 1.0;
    AvellanedaStoikovStrategy strategy(scfg);

    LimitOrderBook book;
    MatchingEngine engine(book);
    static_cast<void>(book.add_order(1, Side::SELL, 10100, k_deep_liquidity));

    MarketTickData tick{};
    tick.timestamp_ns = 1;
    tick.price = 10100;
    tick.quantity = 5;
    tick.side = Side::BUY;

    std::vector<FillData> fills;
    fills.reserve(32);
    OrderId market_oid = 1000;

    for (auto _ : state) {
        Event ev{};
        ev.type = EventType::MarketTick;
        ev.market_tick = tick;
        static_cast<void>(bus.try_push(ev));
        Event popped{};
        static_cast<void>(bus.try_pop(popped));
        benchmark::DoNotOptimize(popped);

        const Price mid = 10100;
        risk.mark(mid);
        static_cast<void>(strategy.optimal_quotes(mid, risk.position(), 0.15, 1.0));

        fills.clear();
        static_cast<void>(engine.submit_order(
            market_oid++, Side::BUY, std::numeric_limits<Price>::max(), tick.quantity, fills));
        benchmark::DoNotOptimize(fills.size());
    }
}
BENCHMARK(BM_Pipeline_TickToTrade_Latency)
    ->Name("pipeline/tick_to_trade")
    ->Unit(benchmark::kNanosecond);

} // namespace

BENCHMARK_MAIN();
