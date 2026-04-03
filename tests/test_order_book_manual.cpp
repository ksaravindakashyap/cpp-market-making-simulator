/**
 * Standalone manual tests for mmsim::LimitOrderBook.
 * Price scale: raw = decimal_price * 10_000 (see mmsim/types.h).
 */

#include "mmsim/order_book.h"
#include "mmsim/types.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <optional>

namespace {

using mmsim::BookSnapshot;
using mmsim::LimitOrderBook;
using mmsim::OrderId;
using mmsim::Price;
using mmsim::PriceLevelSnapshot;
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

[[nodiscard]] double human_price(Price p) {
    return static_cast<double>(p) / 10000.0;
}

[[nodiscard]] Price price_from_human(double x) {
    return static_cast<Price>(std::llround(x * 10000.0));
}

[[nodiscard]] Quantity total_qty_at_best_bid(const LimitOrderBook& book) {
    const auto snap = book.snapshot();
    if (snap.bids.empty()) {
        return 0;
    }
    Quantity sum = 0;
    for (const auto& o : snap.bids[0].orders) {
        sum += o.quantity;
    }
    return sum;
}

/// Full spread in raw price units (ask - bid) when both sides exist.
[[nodiscard]] std::optional<Price> spread_raw(const LimitOrderBook& book) {
    const auto bid = book.best_bid();
    const auto ask = book.best_ask();
    if (!bid.has_value() || !ask.has_value()) {
        return std::nullopt;
    }
    return *ask - *bid;
}

[[nodiscard]] BookSnapshot snapshot_top_levels(const BookSnapshot& full, std::size_t n) {
    BookSnapshot out;
    for (std::size_t i = 0; i < n && i < full.bids.size(); ++i) {
        out.bids.push_back(full.bids[i]);
    }
    for (std::size_t i = 0; i < n && i < full.asks.size(); ++i) {
        out.asks.push_back(full.asks[i]);
    }
    return out;
}

[[nodiscard]] bool bids_descending(const BookSnapshot& s) {
    for (std::size_t i = 1; i < s.bids.size(); ++i) {
        if (s.bids[i - 1].price < s.bids[i].price) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool asks_ascending(const BookSnapshot& s) {
    for (std::size_t i = 1; i < s.asks.size(); ++i) {
        if (s.asks[i - 1].price > s.asks[i].price) {
            return false;
        }
    }
    return true;
}

} // namespace

int main() {
    std::printf("=== test_order_book_manual (LimitOrderBook) ===\n");
    std::printf("Price scale: raw = human * 10000 (e.g. 100.00 -> %lld)\n\n",
                  static_cast<long long>(price_from_human(100.0)));

    LimitOrderBook book;

    const Price p100 = price_from_human(100.0);
    const Price p101 = price_from_human(101.0);
    const Price p995 = price_from_human(99.5);

    // 1. BUY 100.00 x 500
    if (!book.add_order(1, Side::BUY, p100, 500)) {
        fail("add BUY @100");
    } else {
        const auto bb = book.best_bid();
        if (!bb || *bb != p100) {
            fail("1. best_bid should be 100.00");
        } else {
            std::printf("PASS: 1. best_bid() = %.2f (raw %lld)\n", human_price(*bb),
                        static_cast<long long>(*bb));
        }
    }

    // 2. SELL 101.00 x 300
    if (!book.add_order(2, Side::SELL, p101, 300)) {
        fail("add SELL @101");
    } else {
        const auto ba = book.best_ask();
        if (!ba || *ba != p101) {
            fail("2. best_ask should be 101.00");
        } else {
            std::printf("PASS: 2. best_ask() = %.2f (raw %lld)\n", human_price(*ba),
                        static_cast<long long>(*ba));
        }
    }

    // 3. mid_price
    {
        const auto mid = book.mid_price();
        const Price expect_mid = (p100 + p101) / 2;
        if (!mid || *mid != expect_mid) {
            fail("3. mid_price");
        } else {
            std::printf("PASS: 3. mid_price() = %.2f (expected 100.50)\n", human_price(*mid));
        }
    }

    // 4. spread (computed: API has no spread(); ask - bid)
    {
        const auto sp = spread_raw(book);
        const Price expect_sp = p101 - p100;
        if (!sp || *sp != expect_sp) {
            fail("4. spread");
        } else {
            std::printf("PASS: 4. spread() = %.2f (raw diff %lld, expected 1.00)\n",
                        human_price(*sp), static_cast<long long>(*sp));
        }
    }

    // 5. BUY 99.50 x 200 — best bid still 100
    if (!book.add_order(3, Side::BUY, p995, 200)) {
        fail("add BUY @99.5");
    } else {
        const auto bb = book.best_bid();
        if (!bb || *bb != p100) {
            fail("5. best_bid still 100.00");
        } else {
            std::printf("PASS: 5. best_bid() still %.2f after worse bid added\n", human_price(*bb));
        }
    }

    // 6. Another BUY @100.00 x 300 -> 800 at level
    if (!book.add_order(4, Side::BUY, p100, 300)) {
        fail("add second BUY @100");
    } else {
        const Quantity t = total_qty_at_best_bid(book);
        if (t != 800) {
            fail("6. total qty at best bid");
            std::fprintf(stderr, "  got %lld expected 800\n", static_cast<long long>(t));
        } else {
            std::printf("PASS: 6. total quantity at 100.00 = %lld\n", static_cast<long long>(t));
        }
    }

    // 7. Cancel first BUY (id=1) -> 300 left at 100
    if (!book.cancel_order(1)) {
        fail("cancel order 1");
    } else {
        const Quantity t = total_qty_at_best_bid(book);
        if (t != 300) {
            fail("7. qty at 100 after cancel id=1");
            std::fprintf(stderr, "  got %lld expected 300\n", static_cast<long long>(t));
        } else {
            std::printf("PASS: 7. total at 100.00 after cancel first BUY = %lld\n",
                        static_cast<long long>(t));
        }
    }

    // 8. Cancel second BUY @100 (id=4) -> best bid 99.50
    if (!book.cancel_order(4)) {
        fail("cancel order 4");
    } else {
        const auto bb = book.best_bid();
        if (!bb || *bb != p995) {
            fail("8. best_bid should be 99.50");
        } else {
            std::printf("PASS: 8. best_bid() = %.2f after clearing 100.00 level\n", human_price(*bb));
        }
    }

    // 9. Empty book
    if (!book.cancel_order(2)) {
        fail("cancel SELL");
    }
    if (!book.cancel_order(3)) {
        fail("cancel BUY @99.5");
    }
    if (!book.best_bid().has_value() && !book.best_ask().has_value()) {
        std::printf("PASS: 9. empty book: best_bid/best_ask are nullopt (no crash)\n");
    } else {
        fail("9. expected empty best bid/ask");
    }

    // 10. Snapshot depth: 15 levels per side, take top 10
    book.clear();
    for (int i = 0; i < 15; ++i) {
        const Price bp = p100 - static_cast<Price>(i) * 1000; // 0.10 steps
        const OrderId id = 100 + static_cast<OrderId>(i);
        if (!book.add_order(id, Side::BUY, bp, 10 + i)) {
            fail("snapshot: add bid");
        }
    }
    for (int i = 0; i < 15; ++i) {
        const Price ap = p101 + static_cast<Price>(i) * 1000;
        const OrderId id = 200 + static_cast<OrderId>(i);
        if (!book.add_order(id, Side::SELL, ap, 10 + i)) {
            fail("snapshot: add ask");
        }
    }

    const BookSnapshot full = book.snapshot();
    const BookSnapshot top = snapshot_top_levels(full, 10);

    if (top.bids.size() != 10 || top.asks.size() != 10) {
        fail("10. snapshot_top 10: level count");
        std::fprintf(stderr, "  bids %zu asks %zu\n", top.bids.size(), top.asks.size());
    } else if (!bids_descending(top) || !asks_ascending(top)) {
        fail("10. sort order");
    } else {
        std::printf("PASS: 10. snapshot top 10: %zu bid levels, %zu ask levels; bids non-increasing, "
                    "asks non-decreasing\n",
                    top.bids.size(), top.asks.size());
        std::printf("      first bid level %.4f, first ask level %.4f\n", human_price(top.bids[0].price),
                    human_price(top.asks[0].price));
    }

    std::printf("\n--- Summary: %d failure(s) ---\n", g_failures);
    return g_failures > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
