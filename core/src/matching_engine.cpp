#include "mmsim/matching_engine.h"

#include "mmsim/order_book.h"

#include <algorithm>

namespace mmsim {

namespace {

[[nodiscard]] bool self_trade_block(StrategyId resting, StrategyId aggressor) noexcept {
    return resting != 0 && aggressor != 0 && resting == aggressor;
}

} // namespace

MatchingEngine::MatchingEngine(LimitOrderBook& book) : book_{book} {}

bool MatchingEngine::submit_order(OrderId id, Side side, Price price, Quantity quantity,
                                  std::vector<FillData>& fills_out, StrategyId aggressor_strategy) {
    fills_out.clear();

    if (quantity <= 0) {
        return false;
    }
    if (book_.by_id_.find(id) != book_.by_id_.end()) {
        return false;
    }

    Quantity remaining = quantity;

    auto emit_pair = [this, &fills_out](OrderId resting_id, OrderId aggressor_id, Price trade_price,
                                        Quantity trade_qty) {
        const std::uint64_t ts = next_fill_timestamp_ns_++;
        fills_out.push_back(
            FillData{resting_id, trade_price, trade_qty, 0, resting_id, aggressor_id, ts});
        fills_out.push_back(
            FillData{aggressor_id, trade_price, trade_qty, 0, resting_id, aggressor_id, ts});
    };

    if (side == Side::BUY) {
        while (remaining > 0 && !book_.asks_.empty()) {
            const Quantity before = remaining;
            bool limit_hit = false;
            for (auto pit = book_.asks_.begin(); pit != book_.asks_.end() && remaining > 0;) {
                if (pit->first > price) {
                    limit_hit = true;
                    break;
                }
                auto& level = pit->second;
                auto it = level.begin();
                while (it != level.end() && self_trade_block(it->strategy_id, aggressor_strategy)) {
                    ++it;
                }
                if (it == level.end()) {
                    ++pit;
                    continue;
                }
                const auto resting_it = it;
                auto& resting = *resting_it;
                const Quantity trade_qty = std::min(remaining, resting.quantity);
                const Price trade_price = pit->first;

                emit_pair(resting.id, id, trade_price, trade_qty);

                remaining -= trade_qty;
                resting.quantity -= trade_qty;

                if (resting.quantity == 0) {
                    book_.by_id_.erase(resting.id);
                    level.erase(resting_it);
                }

                if (level.empty()) {
                    pit = book_.asks_.erase(pit);
                }
            }
            if (limit_hit) {
                break;
            }
            if (remaining == before) {
                break;
            }
        }
    } else {
        while (remaining > 0 && !book_.bids_.empty()) {
            const Quantity before = remaining;
            bool limit_hit = false;
            for (auto pit = book_.bids_.begin(); pit != book_.bids_.end() && remaining > 0;) {
                if (pit->first < price) {
                    limit_hit = true;
                    break;
                }
                auto& level = pit->second;
                auto it = level.begin();
                while (it != level.end() && self_trade_block(it->strategy_id, aggressor_strategy)) {
                    ++it;
                }
                if (it == level.end()) {
                    ++pit;
                    continue;
                }
                const auto resting_it = it;
                auto& resting = *resting_it;
                const Quantity trade_qty = std::min(remaining, resting.quantity);
                const Price trade_price = pit->first;

                emit_pair(resting.id, id, trade_price, trade_qty);

                remaining -= trade_qty;
                resting.quantity -= trade_qty;

                if (resting.quantity == 0) {
                    book_.by_id_.erase(resting.id);
                    level.erase(resting_it);
                }

                if (level.empty()) {
                    pit = book_.bids_.erase(pit);
                }
            }
            if (limit_hit) {
                break;
            }
            if (remaining == before) {
                break;
            }
        }
    }

    if (remaining > 0) {
        return book_.add_order(id, side, price, remaining, aggressor_strategy);
    }
    return true;
}

} // namespace mmsim
