#pragma once

#include "mmsim/types.h"

#include <cstddef>
#include <functional>
#include <list>
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

namespace mmsim {

class MatchingEngine;

struct OrderAtLevel {
    OrderId order_id = 0;
    Quantity quantity = 0;
};

struct PriceLevelSnapshot {
    Price price = 0;
    std::vector<OrderAtLevel> orders;
};

struct BookSnapshot {
    /// Bid levels from best (highest price) to worst.
    std::vector<PriceLevelSnapshot> bids;
    /// Ask levels from best (lowest price) to worst.
    std::vector<PriceLevelSnapshot> asks;
};

/// Limit order book: bids in descending price order, asks in ascending price order.
/// FIFO within a price level (std::list).
class LimitOrderBook {
    friend class MatchingEngine;

  public:
    LimitOrderBook() = default;

    LimitOrderBook(const LimitOrderBook&) = delete;
    LimitOrderBook& operator=(const LimitOrderBook&) = delete;
    LimitOrderBook(LimitOrderBook&&) = delete;
    LimitOrderBook& operator=(LimitOrderBook&&) = delete;

    ~LimitOrderBook() = default;

    /// Rejects duplicate IDs and non-positive quantity.
    [[nodiscard]] bool add_order(OrderId id, Side side, Price price, Quantity quantity,
                                 StrategyId strategy_id = 0);

    [[nodiscard]] bool cancel_order(OrderId id);

    [[nodiscard]] std::optional<Price> best_bid() const;

    [[nodiscard]] std::optional<Price> best_ask() const;

    /// Arithmetic mean of best bid and best ask when both exist.
    [[nodiscard]] std::optional<Price> mid_price() const;

    [[nodiscard]] BookSnapshot snapshot() const;

    [[nodiscard]] std::size_t order_count() const;

    /// Remove all resting orders (for simulation reset).
    void clear() noexcept;

  private:
    struct BookOrder {
        OrderId id = 0;
        Quantity quantity = 0;
        StrategyId strategy_id = 0;
    };

    using OrderList = std::list<BookOrder>;
    using BidLevels = std::map<Price, OrderList, std::greater<Price>>;
    using AskLevels = std::map<Price, OrderList>;

    struct OrderLocation {
        Side side = Side::BUY;
        Price price = 0;
        OrderList::iterator iter{};
    };

    BidLevels bids_;
    AskLevels asks_;
    std::unordered_map<OrderId, OrderLocation> by_id_;
};

} // namespace mmsim
