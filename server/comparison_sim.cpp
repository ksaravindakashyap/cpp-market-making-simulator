#include "comparison_sim.hpp"

#include "mmsim/avellaneda_stoikov.h"
#include "mmsim/event_types.h"
#include "mmsim/fixed_spread_mm.h"
#include "mmsim/market_data_feed.h"
#include "mmsim/matching_engine.h"
#include "mmsim/order_book.h"
#include "mmsim/order_manager.h"
#include "mmsim/risk_manager.h"

#include "mmsim/event_types.h"

#include <nlohmann/json.hpp>

#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace mmsim::ws {
namespace {

using namespace mmsim;

enum class StrategyKind { AvellanedaStoikov, FixedSpread };

struct RunMetrics {
    std::size_t ticks = 0;
    double equity = 0.0;
    double realized_pnl = 0.0;
    double unrealized_pnl = 0.0;
    double max_drawdown = 0.0;
    std::optional<double> sharpe;
    std::optional<double> sortino;
    double mean_inventory = 0.0;
    double variance_inventory = 0.0;
    Quantity max_abs_inventory = 0;
    Quantity final_position = 0;
};

[[nodiscard]] nlohmann::json metrics_to_json(const RunMetrics& m) {
    nlohmann::json j;
    j["ticks"] = m.ticks;
    j["equity"] = m.equity;
    j["realized_pnl"] = m.realized_pnl;
    j["unrealized_pnl"] = m.unrealized_pnl;
    j["max_drawdown"] = m.max_drawdown;
    j["sharpe_ratio"] = m.sharpe ? nlohmann::json(*m.sharpe) : nlohmann::json(nullptr);
    j["sortino_ratio"] = m.sortino ? nlohmann::json(*m.sortino) : nlohmann::json(nullptr);
    j["mean_inventory"] = m.mean_inventory;
    j["variance_inventory"] = m.variance_inventory;
    j["max_abs_inventory"] = m.max_abs_inventory;
    j["final_position"] = m.final_position;
    return j;
}

[[nodiscard]] std::string fmt_opt(double x) {
    std::ostringstream o;
    o << std::fixed << std::setprecision(4) << x;
    return o.str();
}

[[nodiscard]] std::string fmt_opt_sharpe(const std::optional<double>& x) {
    if (!x.has_value()) {
        return "n/a";
    }
    return fmt_opt(*x);
}

[[nodiscard]] std::string build_markdown(const RunMetrics& as, const RunMetrics& nv,
                                         Price half_spread) {
    std::ostringstream o;
    o << "| Metric | Avellaneda–Stoikov | Fixed spread (" << half_spread << " half) |\n";
    o << "| --- | ---: | ---: |\n";
    o << "| Sharpe (ann.) | " << fmt_opt_sharpe(as.sharpe) << " | " << fmt_opt_sharpe(nv.sharpe)
      << " |\n";
    o << "| Sortino (ann.) | " << fmt_opt_sharpe(as.sortino) << " | " << fmt_opt_sharpe(nv.sortino)
      << " |\n";
    o << "| Max drawdown | " << fmt_opt(as.max_drawdown) << " | " << fmt_opt(nv.max_drawdown)
      << " |\n";
    o << "| Equity | " << fmt_opt(as.equity) << " | " << fmt_opt(nv.equity) << " |\n";
    o << "| Realized PnL | " << fmt_opt(as.realized_pnl) << " | " << fmt_opt(nv.realized_pnl)
      << " |\n";
    o << "| Unrealized PnL | " << fmt_opt(as.unrealized_pnl) << " | " << fmt_opt(nv.unrealized_pnl)
      << " |\n";
    o << "| Mean inventory | " << fmt_opt(as.mean_inventory) << " | " << fmt_opt(nv.mean_inventory)
      << " |\n";
    o << "| Inventory variance | " << fmt_opt(as.variance_inventory) << " | "
      << fmt_opt(nv.variance_inventory) << " |\n";
    o << "| Max |inventory| | " << as.max_abs_inventory << " | " << nv.max_abs_inventory << " |\n";
    o << "| Final position | " << as.final_position << " | " << nv.final_position << " |\n";
    o << "| Ticks | " << as.ticks << " | " << nv.ticks << " |\n";
    return o.str();
}

[[nodiscard]] RunMetrics run_replay(const ReplayOptions& opt, StrategyKind kind) {
    MarketDataFeed feed;
    if (!feed.load_csv(opt.data_path)) {
        return {};
    }
    feed.set_replay_speed(ReplaySpeed::Max);

    const std::size_t total_ticks = feed.tick_count();
    if (total_ticks == 0) {
        return {};
    }

    LimitOrderBook book;
    MatchingEngine engine(book);
    OrderManager orders(book);
    SimulatedMarketOrderFlow market_flow(engine);

    RiskManager::Config rcfg;
    rcfg.max_position = opt.max_position;
    rcfg.periods_per_year = opt.periods_per_year;
    RiskManager risk(rcfg);

    AvellanedaStoikovStrategy strategy(
        AvellanedaStoikovStrategy::Config{.gamma = opt.gamma, .kappa = opt.kappa});

    EventBus bus(opt.bus_capacity);

    std::size_t ticks_processed = 0;

    auto handle_tick = [&](const MarketTickData& t) {
        Price mid = t.price;
        if (const auto bb = book.best_bid(), ba = book.best_ask();
            bb.has_value() && ba.has_value()) {
            mid = (*bb + *ba) / 2;
        }

        risk.mark(mid);

        const double tau_eff = opt.tau_decay
                                   ? opt.tau * (1.0 - static_cast<double>(ticks_processed) /
                                                          static_cast<double>(total_ticks))
                                   : opt.tau;

        std::optional<AsQuotes> q;
        if (kind == StrategyKind::AvellanedaStoikov) {
            q = strategy.optimal_quotes(mid, risk.position(), opt.sigma, tau_eff);
        } else {
            q = fixed_spread_quotes(mid, opt.fixed_half_spread);
        }

        if (q.has_value()) {
            static_cast<void>(
                orders.update_quotes(risk, q->bid, opt.quote_size, q->ask, opt.quote_size));
        }

        const OrderId market_id = orders.allocate_id();
        std::vector<FillData> fills;
        if (market_flow.execute_tick(t, market_id, fills)) {
            risk.on_order_submitted();
        }

        const Side resting_infer = (t.side == Side::BUY) ? Side::SELL : Side::BUY;

        for (std::size_t i = 0; i + 1 < fills.size(); i += 2) {
            const FillData& resting = fills[i];
            const FillData& aggr = fills[i + 1];

            const std::optional<Side> rs = orders.side_for_order(resting.order_id);
            const Side resting_side = rs.value_or(resting_infer);

            risk.on_fill(resting, resting_side, mid);
            risk.on_fill(aggr, t.side, mid);
        }

        ++ticks_processed;
    };

    while (feed.next_index() < feed.tick_count() || !bus.is_empty()) {
        while (feed.try_push_next(bus)) {
            Event ev;
            while (bus.try_pop(ev)) {
                if (ev.type == EventType::MarketTick) {
                    handle_tick(ev.market_tick);
                }
            }
        }
        Event ev;
        while (bus.try_pop(ev)) {
            if (ev.type == EventType::MarketTick) {
                handle_tick(ev.market_tick);
            }
        }
    }

    if (const auto bb = book.best_bid(), ba = book.best_ask(); bb.has_value() && ba.has_value()) {
        risk.mark((*bb + *ba) / 2);
    }

    RunMetrics m;
    m.ticks = ticks_processed;
    m.equity = risk.equity();
    m.realized_pnl = risk.realized_pnl();
    m.unrealized_pnl = risk.unrealized_pnl();
    m.max_drawdown = risk.max_drawdown();
    m.sharpe = risk.sharpe_ratio();
    m.sortino = risk.sortino_ratio();
    m.mean_inventory = risk.mean_inventory();
    m.variance_inventory = risk.variance_inventory();
    m.max_abs_inventory = risk.max_abs_inventory_observed();
    m.final_position = risk.position();
    return m;
}

[[nodiscard]] bool is_valid_metrics(const RunMetrics& m) {
    return m.ticks > 0;
}

} // namespace

nlohmann::json run_strategy_comparison(const ReplayOptions& opt) {
    RunMetrics as = run_replay(opt, StrategyKind::AvellanedaStoikov);
    RunMetrics nv = run_replay(opt, StrategyKind::FixedSpread);

    if (!is_valid_metrics(as) || !is_valid_metrics(nv)) {
        return nlohmann::json{{"error", "replay failed or empty tick file"},
                              {"path", opt.data_path}};
    }

    nlohmann::json out;
    out["avellaneda_stoikov"] = metrics_to_json(as);
    out["fixed_spread"] = metrics_to_json(nv);
    out["markdown"] = build_markdown(as, nv, opt.fixed_half_spread);
    out["ticks"] = as.ticks;
    out["fixed_half_spread"] = opt.fixed_half_spread;
    return out;
}

} // namespace mmsim::ws
