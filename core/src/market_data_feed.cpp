#include "mmsim/market_data_feed.h"

#include "mmsim/matching_engine.h"

#include <chrono>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <thread>

namespace mmsim {

namespace {

[[nodiscard]] std::string trim(std::string_view s) {
    std::size_t a = 0;
    while (a < s.size() && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) {
        ++a;
    }
    std::size_t b = s.size();
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) {
        --b;
    }
    return std::string(s.substr(a, b - a));
}

[[nodiscard]] bool parse_side(std::string_view t, Side& out) {
    const std::string u = trim(t);
    if (u.empty()) {
        return false;
    }
    if (u == "0" || u == "BUY" || u == "buy" || u == "B" || u == "b") {
        out = Side::BUY;
        return true;
    }
    if (u == "1" || u == "SELL" || u == "sell" || u == "S" || u == "s") {
        out = Side::SELL;
        return true;
    }
    return false;
}

[[nodiscard]] double speed_factor(ReplaySpeed s) noexcept {
    switch (s) {
    case ReplaySpeed::OneX:
        return 1.0;
    case ReplaySpeed::FiveX:
        return 5.0;
    case ReplaySpeed::TenX:
        return 10.0;
    case ReplaySpeed::FiftyX:
        return 50.0;
    case ReplaySpeed::HundredX:
        return 100.0;
    case ReplaySpeed::Max:
        return 0.0;
    }
    return 1.0;
}

} // namespace

bool MarketDataFeed::load_csv(std::string_view path) {
    ticks_.clear();
    reset();

    std::ifstream in{std::string(path)};
    if (!in) {
        return false;
    }

    std::string line;
    bool first = true;
    while (std::getline(in, line)) {
        const std::string ttrim = trim(line);
        if (ttrim.empty() || ttrim[0] == '#') {
            continue;
        }

        std::istringstream row(ttrim);
        std::string token;
        std::vector<std::string> cols;
        while (std::getline(row, token, ',')) {
            cols.push_back(trim(token));
        }
        if (cols.size() < 4) {
            continue;
        }

        if (first) {
            first = false;
            const std::string c0 = cols[0];
            if (c0.find("timestamp") != std::string::npos || c0.find("time") != std::string::npos) {
                continue;
            }
        }

        MarketTickData tick{};
        try {
            tick.timestamp_ns = static_cast<std::uint64_t>(std::stoull(cols[0]));
            tick.price = static_cast<Price>(std::stoll(cols[1]));
            tick.quantity = static_cast<Quantity>(std::stoll(cols[2]));
        } catch (...) {
            continue;
        }
        if (!parse_side(cols[3], tick.side)) {
            continue;
        }
        if (tick.quantity <= 0 || tick.price <= 0) {
            continue;
        }
        ticks_.push_back(tick);
    }

    return !ticks_.empty();
}

void MarketDataFeed::reset() noexcept {
    next_index_ = 0;
    slept_for_next_emit_ = false;
}

void MarketDataFeed::sleep_before_index(std::size_t idx) const {
    if (idx == 0 || speed_ == ReplaySpeed::Max) {
        return;
    }
    const double fac = speed_factor(speed_);
    if (fac <= 0.0) {
        return;
    }
    const std::uint64_t t0 = ticks_[idx - 1].timestamp_ns;
    const std::uint64_t t1 = ticks_[idx].timestamp_ns;
    if (t1 <= t0) {
        return;
    }
    const std::uint64_t dt = t1 - t0;
    const auto sleep_ns = static_cast<std::uint64_t>(static_cast<double>(dt) / fac);
    std::this_thread::sleep_for(
        std::chrono::nanoseconds(static_cast<std::chrono::nanoseconds::rep>(sleep_ns)));
}

bool MarketDataFeed::try_push_next(EventBus& bus) {
    if (next_index_ >= ticks_.size()) {
        return false;
    }

    if (next_index_ > 0 && !slept_for_next_emit_) {
        sleep_before_index(next_index_);
        slept_for_next_emit_ = true;
    }

    Event e{};
    e.type = EventType::MarketTick;
    e.market_tick = ticks_[next_index_];

    if (!bus.try_push(e)) {
        return false;
    }

    ++next_index_;
    slept_for_next_emit_ = false;
    return true;
}

SimulatedMarketOrderFlow::SimulatedMarketOrderFlow(MatchingEngine& engine) : engine_{engine} {}

bool SimulatedMarketOrderFlow::execute_tick(const MarketTickData& tick, OrderId order_id,
                                            std::vector<FillData>& fills_out) {
    fills_out.clear();
    if (tick.quantity <= 0) {
        return false;
    }

    constexpr Price k_buy_limit = std::numeric_limits<Price>::max();
    constexpr Price k_sell_limit = 0;

    if (tick.side == Side::BUY) {
        return engine_.submit_order(order_id, Side::BUY, k_buy_limit, tick.quantity, fills_out);
    }
    return engine_.submit_order(order_id, Side::SELL, k_sell_limit, tick.quantity, fills_out);
}

} // namespace mmsim
