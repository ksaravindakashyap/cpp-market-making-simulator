#pragma once

#include "mmsim/event_types.h"
#include "mmsim/types.h"

#include <vector>

namespace mmsim {

class LimitOrderBook;

/// Price-time priority matching: best opposing price first, FIFO within each level.
/// Emits two FillData entries per execution (resting leg + aggressor leg). Fee is zero.
class MatchingEngine {
  public:
    explicit MatchingEngine(LimitOrderBook& book);

    MatchingEngine(const MatchingEngine&) = delete;
    MatchingEngine& operator=(const MatchingEngine&) = delete;

    /// Aggressive limit order: match against the book, then rest any remainder.
    /// Fails if `quantity <= 0`, `id` already exists, or resting append fails.
    /// `aggressor_strategy`: resting orders with the same non-zero strategy do not match (self-trade).
    [[nodiscard]] bool submit_order(OrderId id, Side side, Price price, Quantity quantity,
                                    std::vector<FillData>& fills_out,
                                    StrategyId aggressor_strategy = 0);

  private:
    LimitOrderBook& book_;
    std::uint64_t next_fill_timestamp_ns_ = 1;
};

} // namespace mmsim
