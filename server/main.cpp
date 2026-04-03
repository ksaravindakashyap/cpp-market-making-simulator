#include "mmsim/avellaneda_stoikov.h"
#include "mmsim/event_types.h"
#include "mmsim/market_data_feed.h"
#include "mmsim/matching_engine.h"
#include "mmsim/order_book.h"
#include "mmsim/order_manager.h"
#include "mmsim/risk_manager.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
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
};

void print_usage(std::ostream& os) {
    os << "Usage: mmsim_server [options]\n"
       << "  --data <path>          CSV ticks: timestamp_ns,price,quantity,side\n"
       << "  --speed <1|10|100|max> Replay pacing (default: max)\n"
       << "  --bus-capacity <n>     SPSC ring buffer size (default: 65536)\n"
       << "  --gamma <x>            Avellaneda-Stoikov gamma (default: 0.5)\n"
       << "  --kappa <x>            Avellaneda-Stoikov kappa (default: 1.0)\n"
       << "  --quote-size <n>       Per-side quote quantity (default: 5)\n"
       << "  --sigma <x>            Volatility input to strategy (default: 0.15)\n"
       << "  --tau <x>              Time-to-horizon scale (default: 1.0)\n"
       << "  --no-tau-decay         Keep tau constant (default: decay toward 0)\n"
       << "  --max-position <n>     Risk position limit (default: 500)\n"
       << "  --periods-per-year <n> Annualization for Sharpe/Sortino (default: 252)\n"
       << "  -h, --help             Show this help\n";
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

    const auto t_wall0 = std::chrono::steady_clock::now();
    std::size_t ticks_processed = 0;

    auto handle_tick = [&](const mmsim::MarketTickData& t) {
        mmsim::Price mid = t.price;
        if (const auto bb = book.best_bid(), ba = book.best_ask();
            bb.has_value() && ba.has_value()) {
            mid = (*bb + *ba) / 2;
        }

        risk.mark(mid);

        const double tau_eff = opt.tau_decay
                                   ? opt.tau * (1.0 - static_cast<double>(ticks_processed) /
                                                          static_cast<double>(total_ticks))
                                   : opt.tau;

        const auto q = strategy.optimal_quotes(mid, risk.position(), opt.sigma, tau_eff);
        if (q.has_value()) {
            static_cast<void>(
                orders.update_quotes(risk, q->bid, opt.quote_size, q->ask, opt.quote_size));
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
            risk.on_fill(aggr, t.side, mid);
        }

        ++ticks_processed;
    };

    while (feed.next_index() < feed.tick_count() || !bus.is_empty()) {
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

    const auto t_wall1 = std::chrono::steady_clock::now();
    const double wall_s = std::chrono::duration<double>(t_wall1 - t_wall0).count();

    print_summary(risk, ticks_processed, wall_s);
    return 0;
}
