#include "comparison_sim.hpp"
#include "mmsim/avellaneda_stoikov.h"
#include "mmsim/event_types.h"
#include "mmsim/market_data_feed.h"
#include "mmsim/matching_engine.h"
#include "mmsim/order_book.h"
#include "mmsim/order_manager.h"
#include "mmsim/risk_manager.h"
#include "mmsim/volatility.h"
#include "ws/json_ws.hpp"
#include "ws/ws_hub.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <deque>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

struct SimOptions {
    std::string data_path = "data/sample_ticks.csv";
    mmsim::ReplaySpeed replay_speed = mmsim::ReplaySpeed::Max;
    std::size_t bus_capacity = 65536;

    double gamma = 0.5;
    double kappa = 1.0;
    mmsim::Quantity quote_size = 5;
    double sigma = 0.15;
    double tau = 1.0;
    bool tau_decay = true;

    mmsim::Quantity max_position = 500;
    double periods_per_year = 252.0;
    std::size_t vol_window = 32;

    /// Half-spread (raw price units) for offline fixed-spread comparison baseline.
    mmsim::Price fixed_half_spread = 100;

    std::uint16_t ws_port = 8080;
};

struct LiveSnapshot {
    static constexpr std::size_t kVolBarTicks = 4;
    static constexpr std::size_t kVolHistCap = 48;

    std::mutex mu;
    mmsim::LimitOrderBook* book = nullptr;
    mmsim::RiskManager* risk = nullptr;
    mmsim::OrderManager* orders = nullptr;
    mmsim::AvellanedaStoikovStrategy* strategy = nullptr;
    mmsim::Price last_bid = 0;
    mmsim::Price last_ask = 0;
    double sigma = 0.15;
    mmsim::Quantity inventory = 0;

    mmsim::VolatilityEstimator vol_est{32};
    std::vector<mmsim::Price> vol_closes;
    std::vector<mmsim::OhlcBar> vol_bars;
    std::vector<mmsim::Price> vol_bar_chunk;
    std::deque<double> vol_hist_ctc;
    std::deque<double> vol_hist_park;
    std::deque<double> vol_hist_yz;
};

void print_usage(std::ostream& os) {
    os << "Usage: mmsim_ws_server [options]\n"
       << "  --data <path>          CSV ticks (default: data/sample_ticks.csv)\n"
       << "  --speed <1|10|100|max> Replay pacing (default: max)\n"
       << "  --bus-capacity <n>     SPSC ring buffer size (default: 65536)\n"
       << "  --gamma, --kappa, --quote-size, --sigma, --tau, --no-tau-decay,\n"
       << "  --max-position, --periods-per-year  (same as mmsim_server)\n"
       << "  --port <n>             WebSocket port (default: 8080)\n"
       << "  -h, --help             Show this help\n"
       << "\nWebSocket channels: book (100ms), trades (per fill), pnl (100ms),\n"
       << "strategy (100ms), volatility (100ms), analytics (1s), comparison (report). Control "
          "JSON: {\"channel\":"
          "\"control\",\"command\":\"start|stop|reset|set_speed|set_params|run_comparison|"
          "shutdown\", ...}\n";
}

[[nodiscard]] std::optional<mmsim::ReplaySpeed> parse_speed(std::string_view s) {
    if (s == "1" || s == "1x") {
        return mmsim::ReplaySpeed::OneX;
    }
    if (s == "5" || s == "5x") {
        return mmsim::ReplaySpeed::FiveX;
    }
    if (s == "10" || s == "10x") {
        return mmsim::ReplaySpeed::TenX;
    }
    if (s == "50" || s == "50x") {
        return mmsim::ReplaySpeed::FiftyX;
    }
    if (s == "100" || s == "100x") {
        return mmsim::ReplaySpeed::HundredX;
    }
    if (s == "max" || s == "MAX") {
        return mmsim::ReplaySpeed::Max;
    }
    return std::nullopt;
}

[[nodiscard]] bool parse_args(int argc, char** argv, SimOptions& out) {
    for (int i = 1; i < argc; ++i) {
        std::string_view a = argv[i];
        auto need = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for " << name << '\n';
                return nullptr;
            }
            ++i;
            return argv[i];
        };

        if (a == "--data") {
            const char* v = need("--data");
            if (!v) {
                return false;
            }
            out.data_path = v;
        } else if (a == "--speed") {
            const char* v = need("--speed");
            if (!v) {
                return false;
            }
            const auto sp = parse_speed(v);
            if (!sp) {
                std::cerr << "Invalid --speed (use 1, 10, 100, max)\n";
                return false;
            }
            out.replay_speed = *sp;
        } else if (a == "--bus-capacity") {
            const char* v = need("--bus-capacity");
            if (!v) {
                return false;
            }
            out.bus_capacity = static_cast<std::size_t>(std::stoull(v));
        } else if (a == "--gamma") {
            const char* v = need("--gamma");
            if (!v) {
                return false;
            }
            out.gamma = std::stod(v);
        } else if (a == "--kappa") {
            const char* v = need("--kappa");
            if (!v) {
                return false;
            }
            out.kappa = std::stod(v);
        } else if (a == "--quote-size") {
            const char* v = need("--quote-size");
            if (!v) {
                return false;
            }
            out.quote_size = static_cast<mmsim::Quantity>(std::stoll(v));
        } else if (a == "--sigma") {
            const char* v = need("--sigma");
            if (!v) {
                return false;
            }
            out.sigma = std::stod(v);
        } else if (a == "--tau") {
            const char* v = need("--tau");
            if (!v) {
                return false;
            }
            out.tau = std::stod(v);
        } else if (a == "--no-tau-decay") {
            out.tau_decay = false;
        } else if (a == "--max-position") {
            const char* v = need("--max-position");
            if (!v) {
                return false;
            }
            out.max_position = static_cast<mmsim::Quantity>(std::stoll(v));
        } else if (a == "--periods-per-year") {
            const char* v = need("--periods-per-year");
            if (!v) {
                return false;
            }
            out.periods_per_year = std::stod(v);
        } else if (a == "--port") {
            const char* v = need("--port");
            if (!v) {
                return false;
            }
            const int p = std::stoi(v);
            if (p <= 0 || p > 65535) {
                std::cerr << "Invalid --port\n";
                return false;
            }
            out.ws_port = static_cast<std::uint16_t>(p);
        } else {
            std::cerr << "Unknown argument: " << a << '\n';
            return false;
        }
    }
    return true;
}

void print_summary(const mmsim::RiskManager& risk, std::size_t ticks_processed,
                   double wall_seconds) {
    std::cout << '\n'
              << "========== Simulation summary ==========\n"
              << "Ticks processed:     " << ticks_processed << '\n'
              << "Wall time (s):       " << std::fixed << std::setprecision(3) << wall_seconds
              << '\n'
              << "Final position:      " << risk.position() << '\n'
              << "Realized PnL:        " << risk.realized_pnl() << '\n'
              << "Unrealized PnL:      " << risk.unrealized_pnl() << '\n'
              << "Equity:              " << risk.equity() << '\n'
              << "Max drawdown:        " << risk.max_drawdown() << '\n'
              << "Orders submitted:    " << risk.orders_submitted() << '\n'
              << "Fills recorded:      " << risk.fills_recorded() << '\n'
              << "Fill rate:           " << risk.fill_rate() << '\n'
              << "Avg spread captured: " << risk.average_spread_captured() << '\n'
              << "Mean inventory:      " << risk.mean_inventory() << '\n'
              << "Inventory variance:  " << risk.variance_inventory() << '\n'
              << "Max |inventory|:    " << risk.max_abs_inventory_observed() << '\n';

    const auto sh = risk.sharpe_ratio();
    const auto so = risk.sortino_ratio();
    std::cout << "Sharpe (ann.):       " << (sh ? std::to_string(*sh) : std::string("n/a")) << '\n'
              << "Sortino (ann.):      " << (so ? std::to_string(*so) : std::string("n/a")) << '\n'
              << "========================================\n";
}

void apply_control(const nlohmann::json& j, SimOptions& opt, mmsim::MarketDataFeed& feed,
                   mmsim::RiskManager& risk, mmsim::AvellanedaStoikovStrategy& strategy,
                   LiveSnapshot& live, bool& paused, bool& shutdown) {
    const std::string cmd = j.value("command", "");
    if (cmd == "start") {
        paused = false;
        return;
    }
    if (cmd == "stop") {
        paused = true;
        return;
    }
    if (cmd == "shutdown") {
        shutdown = true;
        return;
    }
    if (cmd == "set_speed") {
        const std::string s = j.value("speed", "max");
        if (const auto sp = parse_speed(s)) {
            feed.set_replay_speed(*sp);
            opt.replay_speed = *sp;
        }
        return;
    }
    if (cmd != "set_params") {
        return;
    }
    if (j.contains("gamma")) {
        const double g = j["gamma"].get<double>();
        opt.gamma = std::clamp(g, 0.001, 1.0);
    }
    if (j.contains("kappa")) {
        const double k = j["kappa"].get<double>();
        opt.kappa = std::clamp(k, 0.1, 10.0);
    }
    if (j.contains("quote_size")) {
        opt.quote_size = j["quote_size"].get<mmsim::Quantity>();
    }
    if (j.contains("sigma")) {
        opt.sigma = j["sigma"].get<double>();
    }
    if (j.contains("tau")) {
        opt.tau = j["tau"].get<double>();
    }
    if (j.contains("tau_decay")) {
        opt.tau_decay = j["tau_decay"].get<bool>();
    }
    if (j.contains("max_position")) {
        const auto mp = j["max_position"].get<mmsim::Quantity>();
        opt.max_position = std::clamp(mp, mmsim::Quantity{10}, mmsim::Quantity{200});
    }
    if (j.contains("periods_per_year")) {
        opt.periods_per_year = j["periods_per_year"].get<double>();
    }
    if (j.contains("vol_window")) {
        std::size_t w = j["vol_window"].get<std::size_t>();
        w = std::clamp(w, std::size_t{10}, std::size_t{500});
        opt.vol_window = w;
        {
            std::lock_guard<std::mutex> lock(live.mu);
            live.vol_est.set_window(w);
            live.vol_closes.clear();
            live.vol_bars.clear();
            live.vol_bar_chunk.clear();
            live.vol_hist_ctc.clear();
            live.vol_hist_park.clear();
            live.vol_hist_yz.clear();
        }
    }
    if (j.contains("fixed_half_spread")) {
        const auto hs = j["fixed_half_spread"].get<mmsim::Price>();
        opt.fixed_half_spread = std::clamp(hs, mmsim::Price{1}, mmsim::Price{100000});
    }
    strategy = mmsim::AvellanedaStoikovStrategy(
        mmsim::AvellanedaStoikovStrategy::Config{.gamma = opt.gamma, .kappa = opt.kappa});
    risk.set_max_position(opt.max_position);
    risk.set_periods_per_year(opt.periods_per_year);
}

} // namespace

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        const std::string_view a = argv[i];
        if (a == "-h" || a == "--help") {
            print_usage(std::cout);
            return 0;
        }
    }

    SimOptions opt;
    if (!parse_args(argc, argv, opt)) {
        std::cerr << "Invalid arguments.\n";
        print_usage(std::cerr);
        return 1;
    }

    mmsim::MarketDataFeed feed;
    if (!feed.load_csv(opt.data_path)) {
        std::cerr << "Failed to load CSV: " << opt.data_path << '\n';
        return 1;
    }
    feed.set_replay_speed(opt.replay_speed);

    const std::size_t total_ticks = feed.tick_count();
    if (total_ticks == 0) {
        std::cerr << "No ticks in file.\n";
        return 1;
    }

    mmsim::LimitOrderBook book;
    mmsim::MatchingEngine engine(book);
    mmsim::OrderManager orders(book);
    mmsim::SimulatedMarketOrderFlow market_flow(engine);

    mmsim::RiskManager::Config rcfg;
    rcfg.max_position = opt.max_position;
    rcfg.periods_per_year = opt.periods_per_year;
    mmsim::RiskManager risk(rcfg);

    mmsim::AvellanedaStoikovStrategy::Config scfg;
    scfg.gamma = opt.gamma;
    scfg.kappa = opt.kappa;
    mmsim::AvellanedaStoikovStrategy strategy(scfg);

    mmsim::EventBus bus(opt.bus_capacity);

    LiveSnapshot live;
    live.book = &book;
    live.risk = &risk;
    live.orders = &orders;
    live.strategy = &strategy;
    live.vol_est.set_window(std::clamp(opt.vol_window, std::size_t{10}, std::size_t{500}));

    mmsim::ws::WsHub hub;
    bool sim_paused = false;
    bool shutdown_requested = false;

    const auto book_fn = [&live]() -> nlohmann::json {
        std::lock_guard<std::mutex> lock(live.mu);
        if (!live.book) {
            return nlohmann::json::object();
        }
        return mmsim::ws::book_to_json(live.book->snapshot());
    };
    const auto pnl_fn = [&live]() -> nlohmann::json {
        std::lock_guard<std::mutex> lock(live.mu);
        if (!live.risk) {
            return nlohmann::json::object();
        }
        return mmsim::ws::pnl_to_json(*live.risk);
    };
    const auto strategy_fn = [&live]() -> nlohmann::json {
        std::lock_guard<std::mutex> lock(live.mu);
        if (!live.strategy || !live.risk || !live.orders) {
            return nlohmann::json::object();
        }
        return mmsim::ws::strategy_to_json(live.last_bid, live.last_ask, live.inventory, live.sigma,
                                           live.strategy->gamma(), live.strategy->kappa(),
                                           live.orders->quote_bid_order_id(),
                                           live.orders->quote_ask_order_id());
    };
    const auto analytics_fn = [&live]() -> nlohmann::json {
        std::lock_guard<std::mutex> lock(live.mu);
        if (!live.risk) {
            return nlohmann::json::object();
        }
        return mmsim::ws::analytics_to_json(*live.risk);
    };
    const auto vol_fn = [&live]() -> nlohmann::json {
        std::lock_guard<std::mutex> lock(live.mu);
        nlohmann::json j;
        auto push_hist = [](nlohmann::json& h, const char* key, const std::deque<double>& d) {
            nlohmann::json a = nlohmann::json::array();
            for (double x : d) {
                a.push_back(x);
            }
            h[key] = std::move(a);
        };
        const auto ctc =
            live.vol_est.close_to_close(std::span<const mmsim::Price>(live.vol_closes));
        const auto park = live.vol_est.parkinson(std::span<const mmsim::OhlcBar>(live.vol_bars));
        const auto yz = live.vol_est.yang_zhang(std::span<const mmsim::OhlcBar>(live.vol_bars));
        j["close_to_close"] = ctc ? nlohmann::json(*ctc) : nlohmann::json(nullptr);
        j["parkinson"] = park ? nlohmann::json(*park) : nlohmann::json(nullptr);
        j["yang_zhang"] = yz ? nlohmann::json(*yz) : nlohmann::json(nullptr);
        nlohmann::json hist = nlohmann::json::object();
        push_hist(hist, "close_to_close", live.vol_hist_ctc);
        push_hist(hist, "parkinson", live.vol_hist_park);
        push_hist(hist, "yang_zhang", live.vol_hist_yz);
        j["history"] = std::move(hist);
        return j;
    };

    hub.start(opt.ws_port, book_fn, pnl_fn, strategy_fn, analytics_fn, vol_fn,
              [](const nlohmann::json&) {});

    const auto t_wall0 = std::chrono::steady_clock::now();
    std::size_t ticks_processed = 0;

    auto handle_tick = [&](const mmsim::MarketTickData& t) {
        mmsim::Price mid = t.price;
        if (const auto bb = book.best_bid(), ba = book.best_ask();
            bb.has_value() && ba.has_value()) {
            mid = (*bb + *ba) / 2;
        }

        risk.mark(mid);

        {
            std::lock_guard<std::mutex> lock(live.mu);
            live.vol_closes.push_back(mid);
            while (live.vol_closes.size() > 512) {
                live.vol_closes.erase(live.vol_closes.begin());
            }
            live.vol_bar_chunk.push_back(mid);
            if (live.vol_bar_chunk.size() >= LiveSnapshot::kVolBarTicks) {
                mmsim::OhlcBar b;
                b.open = live.vol_bar_chunk.front();
                b.close = live.vol_bar_chunk.back();
                const auto [mn_it, mx_it] =
                    std::minmax_element(live.vol_bar_chunk.begin(), live.vol_bar_chunk.end());
                b.low_price = *mn_it;
                b.high = *mx_it;
                live.vol_bars.push_back(b);
                live.vol_bar_chunk.clear();
                while (live.vol_bars.size() > 256) {
                    live.vol_bars.erase(live.vol_bars.begin());
                }
            }
            const auto ctc =
                live.vol_est.close_to_close(std::span<const mmsim::Price>(live.vol_closes));
            const auto park =
                live.vol_est.parkinson(std::span<const mmsim::OhlcBar>(live.vol_bars));
            const auto yz = live.vol_est.yang_zhang(std::span<const mmsim::OhlcBar>(live.vol_bars));
            auto push_hist = [&](std::deque<double>& d, const std::optional<double>& v) {
                if (v) {
                    d.push_back(*v);
                    while (d.size() > LiveSnapshot::kVolHistCap) {
                        d.pop_front();
                    }
                }
            };
            push_hist(live.vol_hist_ctc, ctc);
            push_hist(live.vol_hist_park, park);
            push_hist(live.vol_hist_yz, yz);
        }

        const double tau_eff = opt.tau_decay
                                   ? opt.tau * (1.0 - static_cast<double>(ticks_processed) /
                                                          static_cast<double>(total_ticks))
                                   : opt.tau;

        const auto q = strategy.optimal_quotes(mid, risk.position(), opt.sigma, tau_eff);
        if (q.has_value()) {
            static_cast<void>(
                orders.update_quotes(risk, q->bid, opt.quote_size, q->ask, opt.quote_size));
            {
                std::lock_guard<std::mutex> lock(live.mu);
                live.last_bid = q->bid;
                live.last_ask = q->ask;
                live.sigma = opt.sigma;
                live.inventory = risk.position();
            }
        }

        const mmsim::OrderId market_id = orders.allocate_id();
        std::vector<mmsim::FillData> fills;
        if (market_flow.execute_tick(t, market_id, fills)) {
            risk.on_order_submitted();
        }

        const mmsim::Side resting_infer =
            (t.side == mmsim::Side::BUY) ? mmsim::Side::SELL : mmsim::Side::BUY;

        for (std::size_t i = 0; i + 1 < fills.size(); i += 2) {
            const mmsim::FillData& resting = fills[i];
            const mmsim::FillData& aggr = fills[i + 1];

            const std::optional<mmsim::Side> rs = orders.side_for_order(resting.order_id);
            const mmsim::Side resting_side = rs.value_or(resting_infer);

            risk.on_fill(resting, resting_side, mid);
            hub.post_trade(mmsim::ws::trade_fill_to_json(resting, resting_side, t.timestamp_ns,
                                                         risk.position()));
            risk.on_fill(aggr, t.side, mid);
            hub.post_trade(
                mmsim::ws::trade_fill_to_json(aggr, t.side, t.timestamp_ns, risk.position()));
        }

        ++ticks_processed;
    };

    std::atomic<bool> comparison_busy{false};

    // Wall time for summary: capture when replay drains (not when user idles with WS open).
    auto t_wall_sim_end = t_wall0;
    bool sim_idle_after_run = false;

    while (!shutdown_requested) {
        nlohmann::json ctrl;
        while (hub.try_pop_control(ctrl)) {
            const std::string cmd = ctrl.value("command", "");
            if (cmd == "reset") {
                sim_idle_after_run = false;
                hub.clear_trade_history();
                feed.reset();
                mmsim::Event ev;
                while (bus.try_pop(ev)) {
                }
                book.clear();
                orders.reset();
                risk.reset();
                risk.set_max_position(opt.max_position);
                risk.set_periods_per_year(opt.periods_per_year);
                strategy =
                    mmsim::AvellanedaStoikovStrategy(mmsim::AvellanedaStoikovStrategy::Config{
                        .gamma = opt.gamma, .kappa = opt.kappa});
                ticks_processed = 0;
                sim_paused = false;
                {
                    std::lock_guard<std::mutex> lock(live.mu);
                    live.vol_closes.clear();
                    live.vol_bars.clear();
                    live.vol_bar_chunk.clear();
                    live.vol_hist_ctc.clear();
                    live.vol_hist_park.clear();
                    live.vol_hist_yz.clear();
                    live.vol_est.set_window(
                        std::clamp(opt.vol_window, std::size_t{10}, std::size_t{500}));
                }
                continue;
            }
            if (cmd == "run_comparison") {
                if (comparison_busy.exchange(true)) {
                    hub.broadcast_channel("comparison",
                                          nlohmann::json{{"error", "comparison_already_running"}});
                    continue;
                }
                mmsim::ws::ReplayOptions ro;
                ro.data_path = opt.data_path;
                ro.bus_capacity = opt.bus_capacity;
                ro.gamma = opt.gamma;
                ro.kappa = opt.kappa;
                ro.quote_size = opt.quote_size;
                ro.sigma = opt.sigma;
                ro.tau = opt.tau;
                ro.tau_decay = opt.tau_decay;
                ro.max_position = opt.max_position;
                ro.periods_per_year = opt.periods_per_year;
                ro.fixed_half_spread = ctrl.value("fixed_half_spread", opt.fixed_half_spread);
                std::thread([&hub, ro, &comparison_busy]() {
                    try {
                        const nlohmann::json j = mmsim::ws::run_strategy_comparison(ro);
                        hub.broadcast_channel("comparison", j);
                    } catch (...) {
                        hub.broadcast_channel("comparison",
                                              nlohmann::json{{"error", "comparison_failed"}});
                    }
                    comparison_busy.store(false);
                }).detach();
                continue;
            }
            apply_control(ctrl, opt, feed, risk, strategy, live, sim_paused, shutdown_requested);
        }
        if (shutdown_requested) {
            break;
        }

        const bool sim_has_work = feed.next_index() < feed.tick_count() || !bus.is_empty();

        // Replay finished: keep WebSocket up so the dashboard can show last state and reconnect
        // does not spin (process exits immediately after a tiny CSV otherwise).
        if (!sim_has_work) {
            if (!sim_idle_after_run) {
                sim_idle_after_run = true;
                t_wall_sim_end = std::chrono::steady_clock::now();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }

        if (sim_paused) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        while (feed.try_push_next(bus)) {
            mmsim::Event ev;
            while (bus.try_pop(ev)) {
                if (ev.type == mmsim::EventType::MarketTick) {
                    handle_tick(ev.market_tick);
                }
            }
        }
        mmsim::Event ev;
        while (bus.try_pop(ev)) {
            if (ev.type == mmsim::EventType::MarketTick) {
                handle_tick(ev.market_tick);
            }
        }
    }

    if (const auto bb = book.best_bid(), ba = book.best_ask(); bb.has_value() && ba.has_value()) {
        risk.mark((*bb + *ba) / 2);
    }

    hub.stop();

    const auto t_wall_end = sim_idle_after_run ? t_wall_sim_end : std::chrono::steady_clock::now();
    const double wall_s = std::chrono::duration<double>(t_wall_end - t_wall0).count();

    print_summary(risk, ticks_processed, wall_s);
    return 0;
}
