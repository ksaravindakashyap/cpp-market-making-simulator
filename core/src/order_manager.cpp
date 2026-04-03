#include "mmsim/order_manager.h"

#include "mmsim/order_book.h"

namespace mmsim {

OrderManager::OrderManager(LimitOrderBook& book) : book_{book} {}

OrderId OrderManager::allocate_id() {
    return next_id_++;
}

void OrderManager::erase_side(OrderId id) {
    sides_.erase(id);
}

void OrderManager::cancel_quotes() {
    if (bid_order_.has_value()) {
        static_cast<void>(book_.cancel_order(*bid_order_));
        erase_side(*bid_order_);
        bid_order_.reset();
    }
    if (ask_order_.has_value()) {
        static_cast<void>(book_.cancel_order(*ask_order_));
        erase_side(*ask_order_);
        ask_order_.reset();
    }
}

std::optional<Side> OrderManager::side_for_order(OrderId id) const {
    const auto it = sides_.find(id);
    if (it == sides_.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool OrderManager::update_quotes(RiskManager& risk, Price bid_px, Quantity bid_qty, Price ask_px,
                                 Quantity ask_qty) {
    cancel_quotes();

    if (!risk.allow_order(Side::BUY, bid_qty)) {
        return false;
    }
    if (!risk.allow_order(Side::SELL, ask_qty)) {
        return false;
    }

    const OrderId bid = allocate_id();
    const OrderId ask = allocate_id();

    if (!book_.add_order(bid, Side::BUY, bid_px, bid_qty)) {
        return false;
    }
    sides_[bid] = Side::BUY;

    if (!book_.add_order(ask, Side::SELL, ask_px, ask_qty)) {
        static_cast<void>(book_.cancel_order(bid));
        erase_side(bid);
        return false;
    }
    sides_[ask] = Side::SELL;

    bid_order_ = bid;
    ask_order_ = ask;

    risk.on_order_submitted();
    risk.on_order_submitted();
    return true;
}

void OrderManager::reset() noexcept {
    bid_order_.reset();
    ask_order_.reset();
    sides_.clear();
    next_id_ = 1;
}

} // namespace mmsim
