#pragma once

#include "mmsim/event_types.h"
#include "mmsim/order_book.h"
#include "mmsim/risk_manager.h"
#include "mmsim/types.h"

#include <cstdint>
#include <nlohmann/json.hpp>
#include <optional>

namespace mmsim::ws {

inline nlohmann::json price_level_to_json(const PriceLevelSnapshot& pl) {
    nlohmann::json orders = nlohmann::json::array();
    for (const auto& o : pl.orders) {
        orders.push_back({{"order_id", o.order_id}, {"quantity", o.quantity}});
    }
    return {{"price", pl.price}, {"orders", std::move(orders)}};
}

inline nlohmann::json book_to_json(const BookSnapshot& s) {
    nlohmann::json bids = nlohmann::json::array();
    for (const auto& b : s.bids) {
        bids.push_back(price_level_to_json(b));
    }
    nlohmann::json asks = nlohmann::json::array();
    for (const auto& a : s.asks) {
        asks.push_back(price_level_to_json(a));
    }
    return {{"bids", std::move(bids)}, {"asks", std::move(asks)}};
}

inline nlohmann::json fill_to_json(const FillData& f) {
    return {{"order_id", f.order_id}, {"price", f.price}, {"quantity", f.quantity}, {"fee", f.fee}};
}

/// Fill plus trade log fields for WebSocket `trades` channel.
inline nlohmann::json trade_fill_to_json(const FillData& f, Side side, std::uint64_t timestamp_ns,
                                         Quantity inventory_after) {
    nlohmann::json j = fill_to_json(f);
    j["side"] = (side == Side::BUY) ? "BUY" : "SELL";
    j["timestamp_ns"] = timestamp_ns;
    j["inventory_after"] = inventory_after;
    return j;
}

inline nlohmann::json pnl_to_json(const RiskManager& r) {
    return {{"position", r.position()},
            {"realized_pnl", r.realized_pnl()},
            {"unrealized_pnl", r.unrealized_pnl()},
            {"equity", r.equity()},
            {"max_drawdown", r.max_drawdown()},
            {"max_position", r.config().max_position}};
}

inline nlohmann::json strategy_to_json(Price bid, Price ask, Quantity inventory, double sigma,
                                       double gamma, double kappa,
                                       std::optional<OrderId> bid_order_id = std::nullopt,
                                       std::optional<OrderId> ask_order_id = std::nullopt) {
    nlohmann::json j = {{"bid", bid},     {"ask", ask},     {"inventory", inventory},
                        {"sigma", sigma}, {"gamma", gamma}, {"kappa", kappa}};
    if (bid_order_id.has_value()) {
        j["bid_order_id"] = *bid_order_id;
    } else {
        j["bid_order_id"] = nullptr;
    }
    if (ask_order_id.has_value()) {
        j["ask_order_id"] = *ask_order_id;
    } else {
        j["ask_order_id"] = nullptr;
    }
    return j;
}

inline nlohmann::json analytics_to_json(const RiskManager& r) {
    nlohmann::json j = {{"position", r.position()},
                        {"realized_pnl", r.realized_pnl()},
                        {"unrealized_pnl", r.unrealized_pnl()},
                        {"equity", r.equity()},
                        {"max_drawdown", r.max_drawdown()},
                        {"fill_rate", r.fill_rate()},
                        {"average_spread_captured", r.average_spread_captured()},
                        {"mean_inventory", r.mean_inventory()},
                        {"variance_inventory", r.variance_inventory()},
                        {"max_abs_inventory", r.max_abs_inventory_observed()},
                        {"orders_submitted", r.orders_submitted()},
                        {"fills_recorded", r.fills_recorded()}};
    if (const auto sh = r.sharpe_ratio()) {
        j["sharpe_ratio"] = *sh;
    } else {
        j["sharpe_ratio"] = nullptr;
    }
    if (const auto so = r.sortino_ratio()) {
        j["sortino_ratio"] = *so;
    } else {
        j["sortino_ratio"] = nullptr;
    }
    return j;
}

} // namespace mmsim::ws
