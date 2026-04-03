/**
 * MatchingEngine edge cases: self-trade prevention, exact/partial fills, no match,
 * multi-level sweep, and FillData (maker/taker/timestamp) validation.
 */



#include "mmsim/matching_engine.h"

#include "mmsim/order_book.h"

#include "mmsim/types.h"



#include <cmath>

#include <cstdio>

#include <cstdlib>

#include <vector>



namespace {



using mmsim::FillData;

using mmsim::LimitOrderBook;

using mmsim::MatchingEngine;

using mmsim::OrderId;

using mmsim::Price;

using mmsim::Quantity;

using mmsim::Side;

using mmsim::StrategyId;



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



[[nodiscard]] bool verify_fill_leg(const FillData& f, OrderId order_id, OrderId maker_id,

                                   OrderId taker_id, Price price, Quantity qty,

                                   std::uint64_t expected_ts) {

    if (f.order_id != order_id || f.maker_id != maker_id || f.taker_id != taker_id ||

        f.price != price || f.quantity != qty || f.timestamp_ns != expected_ts || f.fee != 0) {

        return false;

    }

    return true;

}

[[nodiscard]] Quantity ask_qty_at_price(const LimitOrderBook& book, Price price) {

    for (const auto& lev : book.snapshot().asks) {

        if (lev.price == price) {

            Quantity s = 0;

            for (const auto& o : lev.orders) {

                s += o.quantity;

            }

            return s;

        }

    }

    return 0;

}



} // namespace



int main() {

    std::printf("=== test_matching_edge_cases_manual ===\n\n");



    const Price p49 = px(49.0);

    const Price p50 = px(50.0);

    const Price p51 = px(51.0);

    const Price p100 = px(100.0);

    const Price p101 = px(101.0);

    const Price p102 = px(102.0);

    const Price p103 = px(103.0);



    // --- 1. Self-trade prevention (same non-zero strategy) ---

    {

        LimitOrderBook book;

        MatchingEngine engine(book);

        constexpr StrategyId kStrat = 42;

        std::vector<FillData> fills;



        if (!book.add_order(1, Side::SELL, p50, 100, kStrat)) {

            fail("1. add_order SELL strat");

            return EXIT_FAILURE;

        }

        if (!engine.submit_order(2, Side::BUY, p50, 100, fills, kStrat)) {

            fail("1. submit_order BUY strat");

            return EXIT_FAILURE;

        }

        if (!fills.empty()) {

            fail("1. self-trade: expected no fills");

        }

        if (book.order_count() != 2u) {

            fail("1. self-trade: both orders should rest");

        } else {

            pass("1. Self-trade prevention (same strategy, no match)");

        }



        // Cross with different strategy should match

        LimitOrderBook book2;

        MatchingEngine engine2(book2);

        std::vector<FillData> fills2;

        if (!book2.add_order(10, Side::SELL, p50, 50, kStrat)) {

            fail("1b. add_order");

            return EXIT_FAILURE;

        }

        if (!engine2.submit_order(11, Side::BUY, p50, 50, fills2, 99u)) {

            fail("1b. submit");

            return EXIT_FAILURE;

        }

        if (fills2.size() != 2u || fills2[0].quantity != 50) {

            fail("1b. different strategy should match");

        } else {

            pass("1b. Different strategy crosses normally");

        }

    }



    std::printf("\n");



    // --- 2. Exact fill, book empty ---

    {

        LimitOrderBook book;

        MatchingEngine engine(book);

        std::vector<FillData> fills;

        if (!book.add_order(1, Side::BUY, p50, 100)) {

            fail("2. add BUY");

            return EXIT_FAILURE;

        }

        if (!engine.submit_order(2, Side::SELL, p50, 100, fills)) {

            fail("2. submit SELL");

            return EXIT_FAILURE;

        }

        if (fills.size() != 2u || book.order_count() != 0u) {

            fail("2. exact fill / empty book");

        } else {

            pass("2. Exact fill @50, book empty");

        }

        if (fills.size() >= 2u) {

            const std::uint64_t ts = fills[0].timestamp_ns;

            if (!verify_fill_leg(fills[0], 1, 1, 2, p50, 100, ts) ||

                !verify_fill_leg(fills[1], 2, 1, 2, p50, 100, ts) || ts == 0) {

                fail("2. FillData maker/taker/timestamp");

            } else {

                pass("2. FillData fields (maker, taker, price, qty, timestamp)");

            }

        }

    }



    std::printf("\n");



    // --- 3. Partial fill ---

    {

        LimitOrderBook book;

        MatchingEngine engine(book);

        std::vector<FillData> fills;

        if (!book.add_order(1, Side::BUY, p50, 200)) {

            fail("3. add BUY");

            return EXIT_FAILURE;

        }

        if (!engine.submit_order(2, Side::SELL, p50, 100, fills)) {

            fail("3. submit SELL");

            return EXIT_FAILURE;

        }

        if (fills.size() != 2u || fills[0].quantity != 100) {

            fail("3. fill size/qty");

        }

        const auto snap = book.snapshot();

        if (book.order_count() != 1u || snap.bids.size() != 1u ||

            snap.bids[0].orders[0].order_id != 1 || snap.bids[0].orders[0].quantity != 100) {

            fail("3. BUY remainder 100");

        } else {

            pass("3. Partial fill: SELL fully filled, BUY 100 left");

        }

    }



    std::printf("\n");



    // --- 4. Multiple fills against one large bid ---

    {

        LimitOrderBook book;

        MatchingEngine engine(book);

        std::vector<FillData> fills;

        if (!book.add_order(1, Side::BUY, p50, 500)) {

            fail("4. add BUY");

            return EXIT_FAILURE;

        }

        if (!engine.submit_order(2, Side::SELL, p50, 100, fills)) {

            fail("4. SELL 2");

            return EXIT_FAILURE;

        }

        if (fills.size() != 2u || fills[0].quantity != 100) {

            fail("4. first SELL");

        }

        if (!engine.submit_order(3, Side::SELL, p50, 100, fills)) {

            fail("4. SELL 3");

            return EXIT_FAILURE;

        }

        if (fills.size() != 2u || fills[0].quantity != 100) {

            fail("4. second SELL");

        }

        if (!engine.submit_order(4, Side::SELL, p50, 100, fills)) {

            fail("4. SELL 4");

            return EXIT_FAILURE;

        }

        if (fills.size() != 2u || fills[0].quantity != 100) {

            fail("4. third SELL");

        }

        const auto snap = book.snapshot();

        if (book.order_count() != 1u || snap.bids.size() != 1u ||

            snap.bids[0].orders[0].order_id != 1 || snap.bids[0].orders[0].quantity != 200) {

            fail("4. BUY should have 200 left");

        } else {

            pass("4. Three SELLs 100 each vs BUY 500: 200 remaining on BUY");

        }

    }



    std::printf("\n");



    // --- 5. No match ---

    {

        LimitOrderBook book;

        MatchingEngine engine(book);

        std::vector<FillData> fills;

        if (!book.add_order(1, Side::BUY, p49, 100)) {

            fail("5. BUY");

            return EXIT_FAILURE;

        }

        if (!engine.submit_order(2, Side::SELL, p51, 100, fills)) {

            fail("5. SELL");

            return EXIT_FAILURE;

        }

        if (!fills.empty()) {

            fail("5. no fills expected");

        }

        if (book.order_count() != 2u || book.best_bid() != p49 || book.best_ask() != p51) {

            fail("5. both rest, bid 49 ask 51");

        } else {

            pass("5. No cross: both sides rest");

        }

    }



    std::printf("\n");



    // --- 6. Multi-level sweep (buy lifts asks 100, 101, 102) ---

    {

        LimitOrderBook book;

        MatchingEngine engine(book);

        std::vector<FillData> fills;

        if (!book.add_order(1, Side::SELL, p100, 100)) {

            fail("6. ask 100");

            return EXIT_FAILURE;

        }

        if (!book.add_order(2, Side::SELL, p101, 100)) {

            fail("6. ask 101");

            return EXIT_FAILURE;

        }

        if (!book.add_order(3, Side::SELL, p102, 100)) {

            fail("6. ask 102");

            return EXIT_FAILURE;

        }

        if (!engine.submit_order(4, Side::BUY, p103, 250, fills)) {

            fail("6. BUY");

            return EXIT_FAILURE;

        }

        // 3 trades -> 6 FillData

        if (fills.size() != 6u) {

            fail("6. fill count");

        } else {

            if (fills[0].price != p100 || fills[0].quantity != 100 || fills[2].price != p101 ||

                fills[2].quantity != 100 || fills[4].price != p102 || fills[4].quantity != 50) {

                fail("6. sweep prices/qtys");

            } else if (ask_qty_at_price(book, p102) != 50) {

                fail("6. 50 left @102");

            } else {

                pass("6. Sweep: 100@100, 100@101, 50@102; 50 left @102");

            }

        }

    }



    std::printf("\n");



    // --- 7. Fill struct consistency across a batch ---

    {

        LimitOrderBook book;

        MatchingEngine engine(book);

        std::vector<FillData> fills;

        if (!book.add_order(1, Side::SELL, p50, 10)) {

            fail("7. add");

            return EXIT_FAILURE;

        }

        if (!engine.submit_order(2, Side::BUY, p50, 10, fills)) {

            fail("7. submit");

            return EXIT_FAILURE;

        }

        if (fills.size() != 2u) {

            fail("7. size");

        } else {

            const std::uint64_t ts = fills[0].timestamp_ns;

            bool ok = verify_fill_leg(fills[0], 1, 1, 2, p50, 10, ts) &&

                      verify_fill_leg(fills[1], 2, 1, 2, p50, 10, ts) && fills[0].timestamp_ns == fills[1].timestamp_ns;

            if (!ok) {

                fail("7. FillData maker_id, taker_id, price, quantity, timestamp");

            } else {

                pass("7. FillData legs share timestamp; maker=taker ids consistent");

            }

        }

    }



    std::printf("\n--- Summary: %d failure(s) ---\n", g_failures);

    return g_failures > 0 ? EXIT_FAILURE : EXIT_SUCCESS;

}


