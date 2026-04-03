/**
 * Price–time priority: best bid price first, then FIFO within a price level.
 * Sell aggressor at limit 99.00 matches all bids >= 99.00 (see MatchingEngine).
 */

#include "mmsim/matching_engine.h"
#include "mmsim/order_book.h"
#include "mmsim/types.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <vector>

namespace {

using mmsim::FillData;
using mmsim::LimitOrderBook;
using mmsim::MatchingEngine;
using mmsim::OrderId;
using mmsim::Price;
using mmsim::Quantity;
using mmsim::Side;

int g_failures = 0;

void pass(const char* msg) {
    std::printf("PASS: %s\n", msg);
}

void fail(const char* msg) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_failures;
}

[[nodiscard]] Price px(double human) {
    return static_cast<Price>(std::llround(human * 10000.0));
}

[[nodiscard]] Quantity qty_at_best_bid(const LimitOrderBook& book) {
    const auto s = book.snapshot();
    if (s.bids.empty()) {
        return 0;
    }
    Quantity sum = 0;
    for (const auto& o : s.bids[0].orders) {
        sum += o.quantity;
    }
    return sum;
}

[[nodiscard]] std::optional<Quantity> qty_for_order_at_price(const LimitOrderBook& book, OrderId id,
                                                             Price price) {
    const auto s = book.snapshot();
    for (const auto& lev : s.bids) {
        if (lev.price != price) {
            continue;
        }
        for (const auto& o : lev.orders) {
            if (o.order_id == id) {
                return o.quantity;
            }
        }
    }
    return std::nullopt;
}

} // namespace

int main() {
    std::printf("=== test_order_book_price_time_manual (price–time priority) ===\n\n");

    const Price p100 = px(100.0);
    const Price p10050 = px(100.5);
    const Price p99 = px(99.0);

    LimitOrderBook book;
    MatchingEngine engine(book);

    // 1. Resting BUYs: A @100, B @100.50, C @100 (after A)
    if (!book.add_order(1, Side::BUY, p100, 100)) {
        fail("add Order A");
        return EXIT_FAILURE;
    }
    if (!book.add_order(2, Side::BUY, p10050, 100)) {
        fail("add Order B");
        return EXIT_FAILURE;
    }
    if (!book.add_order(3, Side::BUY, p100, 100)) {
        fail("add Order C");
        return EXIT_FAILURE;
    }

    std::printf("Resting: A id=1 @100.00 qty100, B id=2 @100.50 qty100, C id=3 @100.00 qty100\n");

    // 2. Aggressive SELL 250 @ min price 99.00 (matches all bid levels >= 99)
    std::vector<FillData> fills;
    const OrderId sell_id = 4;
    if (!engine.submit_order(sell_id, Side::SELL, p99, 250, fills)) {
        fail("submit_order SELL");
        return EXIT_FAILURE;
    }

    // Fills are pairs: resting leg, aggressor leg per trade
    const std::size_t expected_pairs = 3;
    if (fills.size() != 2 * expected_pairs) {
        fail("fill count");
        std::fprintf(stderr, "  got %zu entries, expected 6\n", fills.size());
    } else {
        std::printf("Got %zu fill records (3 trades x 2 legs)\n", fills.size());
    }

    bool order_ok = true;
    if (fills.size() >= 6) {
        // First trade: B @ 100.50, 100
        if (fills[0].order_id != 2 || fills[0].price != p10050 || fills[0].quantity != 100) {
            fail("fill 1 resting: expected B @100.50 x100");
            order_ok = false;
        }
        if (fills[1].order_id != sell_id || fills[1].quantity != 100) {
            fail("fill 1 aggressor");
            order_ok = false;
        }
        // Second: A @ 100.00, 100
        if (fills[2].order_id != 1 || fills[2].price != p100 || fills[2].quantity != 100) {
            fail("fill 2 resting: expected A @100.00 x100");
            order_ok = false;
        }
        if (fills[3].order_id != sell_id || fills[3].quantity != 100) {
            fail("fill 2 aggressor");
            order_ok = false;
        }
        // Third: C @ 100.00, 50 partial
        if (fills[4].order_id != 3 || fills[4].price != p100 || fills[4].quantity != 50) {
            fail("fill 3 resting: expected C @100.00 x50");
            order_ok = false;
        }
        if (fills[5].order_id != sell_id || fills[5].quantity != 50) {
            fail("fill 3 aggressor");
            order_ok = false;
        }
    } else {
        order_ok = false;
    }

    if (order_ok) {
        pass("3. Match order: B full @100.50, then A full @100.00, then C partial 50 @100.00");
    }

    // 4. Order C remaining qty 50
    const auto cq = qty_for_order_at_price(book, 3, p100);
    if (!cq.has_value() || *cq != 50) {
        fail("4. Order C remaining qty 50");
        if (cq.has_value()) {
            std::fprintf(stderr, "  got %lld\n", static_cast<long long>(*cq));
        }
    } else {
        pass("4. Order C remaining quantity = 50");
    }

    // 5. Best bid 100.00, total qty 50
    const auto bb = book.best_bid();
    const Quantity level_qty = qty_at_best_bid(book);
    if (!bb || *bb != p100 || level_qty != 50) {
        fail("5. best_bid @100.00 with total qty 50");
        std::fprintf(stderr, "  best_bid raw %lld, level qty %lld\n",
                     bb.has_value() ? static_cast<long long>(*bb) : -1LL,
                     static_cast<long long>(level_qty));
    } else {
        pass("5. best_bid = 100.00, aggregate qty at level = 50");
    }

    std::printf("\n--- Summary: %d failure(s) ---\n", g_failures);
    return g_failures > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
