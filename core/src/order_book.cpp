#include "mmsim/order_book.h"

namespace mmsim {

bool LimitOrderBook::add_order(OrderId id, Side side, Price price, Quantity quantity,
                               StrategyId strategy_id) {
    if (quantity <= 0) {
        return false;
    }
    if (by_id_.find(id) != by_id_.end()) {
        return false;
    }

    if (side == Side::BUY) {
        OrderList& level = bids_[price];
        level.push_back(BookOrder{id, quantity, strategy_id});
        auto iter = std::prev(level.end());
        by_id_.emplace(id, OrderLocation{Side::BUY, price, iter});
    } else {
        OrderList& level = asks_[price];
        level.push_back(BookOrder{id, quantity, strategy_id});
        auto iter = std::prev(level.end());
        by_id_.emplace(id, OrderLocation{Side::SELL, price, iter});
    }
    return true;
}

bool LimitOrderBook::cancel_order(OrderId id) {
    auto found = by_id_.find(id);
    if (found == by_id_.end()) {
        return false;
    }

    OrderLocation loc = found->second;
    by_id_.erase(found);

    if (loc.side == Side::BUY) {
        auto level_it = bids_.find(loc.price);
        if (level_it == bids_.end()) {
            return false;
        }
        level_it->second.erase(loc.iter);
        if (level_it->second.empty()) {
            bids_.erase(level_it);
        }
    } else {
        auto level_it = asks_.find(loc.price);
        if (level_it == asks_.end()) {
            return false;
        }
        level_it->second.erase(loc.iter);
        if (level_it->second.empty()) {
            asks_.erase(level_it);
        }
    }
    return true;
}

std::optional<Price> LimitOrderBook::best_bid() const {
    if (bids_.empty()) {
        return std::nullopt;
    }
    return bids_.begin()->first;
}

std::optional<Price> LimitOrderBook::best_ask() const {
    if (asks_.empty()) {
        return std::nullopt;
    }
    return asks_.begin()->first;
}

std::optional<Price> LimitOrderBook::mid_price() const {
    const auto bid = best_bid();
    const auto ask = best_ask();
    if (!bid.has_value() || !ask.has_value()) {
        return std::nullopt;
    }
    return (*bid + *ask) / 2;
}

BookSnapshot LimitOrderBook::snapshot() const {
    BookSnapshot out;
    out.bids.reserve(bids_.size());
    out.asks.reserve(asks_.size());

    for (const auto& [price, orders] : bids_) {
        PriceLevelSnapshot level;
        level.price = price;
        level.orders.reserve(orders.size());
        for (const auto& o : orders) {
            level.orders.push_back(OrderAtLevel{o.id, o.quantity});
        }
        out.bids.push_back(std::move(level));
    }

    for (const auto& [price, orders] : asks_) {
        PriceLevelSnapshot level;
        level.price = price;
        level.orders.reserve(orders.size());
        for (const auto& o : orders) {
            level.orders.push_back(OrderAtLevel{o.id, o.quantity});
        }
        out.asks.push_back(std::move(level));
    }

    return out;
}

std::size_t LimitOrderBook::order_count() const {
    return by_id_.size();
}

void LimitOrderBook::clear() noexcept {
    bids_.clear();
    asks_.clear();
    by_id_.clear();
}

} // namespace mmsim
