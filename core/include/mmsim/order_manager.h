#pragma once

#include "mmsim/risk_manager.h"
#include "mmsim/types.h"

#include <optional>
#include <unordered_map>

namespace mmsim {

class LimitOrderBook;

/// Places and replaces bid/ask quote orders on the book; tracks IDs for fill attribution.
class OrderManager {
  public:
    explicit OrderManager(LimitOrderBook& book);

    [[nodiscard]] OrderId allocate_id();

    /// Cancels prior quote orders and places new bid/ask if risk checks pass.
    [[nodiscard]] bool update_quotes(RiskManager& risk, Price bid_px, Quantity bid_qty,
                                     Price ask_px, Quantity ask_qty);

    void cancel_quotes();

    /// Clear quote tracking and reset ID allocator (after `LimitOrderBook::clear()`).
    void reset() noexcept;

    [[nodiscard]] std::optional<Side> side_for_order(OrderId id) const;

    /// Active quote resting order IDs (after last successful `update_quotes`), if any.
    [[nodiscard]] std::optional<OrderId> quote_bid_order_id() const noexcept {
        return bid_order_;
    }

    [[nodiscard]] std::optional<OrderId> quote_ask_order_id() const noexcept {
        return ask_order_;
    }

    [[nodiscard]] LimitOrderBook& book() noexcept {
        return book_;
    }

  private:
    void erase_side(OrderId id);

    LimitOrderBook& book_;
    std::optional<OrderId> bid_order_;
    std::optional<OrderId> ask_order_;
    OrderId next_id_{1};
    std::unordered_map<OrderId, Side> sides_;
};

} // namespace mmsim
