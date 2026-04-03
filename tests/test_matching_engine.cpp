#include "mmsim/matching_engine.h"
#include "mmsim/order_book.h"
#include "mmsim/types.h"

#include <gtest/gtest.h>

namespace {

using mmsim::FillData;
using mmsim::LimitOrderBook;
using mmsim::MatchingEngine;
using mmsim::Side;
using mmsim::StrategyId;

TEST(MatchingEngine, FullFillNoRest) {
    LimitOrderBook book;
    MatchingEngine engine(book);
    std::vector<FillData> fills;

    ASSERT_TRUE(book.add_order(1, Side::SELL, 10000, 100));

    ASSERT_TRUE(engine.submit_order(2, Side::BUY, 10000, 100, fills));
    ASSERT_EQ(fills.size(), 2u);
    EXPECT_EQ(fills[0].order_id, 1u);
    EXPECT_EQ(fills[0].price, 10000);
    EXPECT_EQ(fills[0].quantity, 100);
    EXPECT_EQ(fills[0].maker_id, 1u);
    EXPECT_EQ(fills[0].taker_id, 2u);
    EXPECT_EQ(fills[0].timestamp_ns, fills[1].timestamp_ns);
    EXPECT_GT(fills[0].timestamp_ns, 0u);
    EXPECT_EQ(fills[1].order_id, 2u);
    EXPECT_EQ(fills[1].quantity, 100);
    EXPECT_EQ(fills[1].maker_id, 1u);
    EXPECT_EQ(fills[1].taker_id, 2u);

    EXPECT_EQ(book.order_count(), 0u);
    EXPECT_FALSE(book.best_bid().has_value());
    EXPECT_FALSE(book.best_ask().has_value());
}

TEST(MatchingEngine, PartialFillThenRest) {
    LimitOrderBook book;
    MatchingEngine engine(book);
    std::vector<FillData> fills;

    ASSERT_TRUE(book.add_order(1, Side::SELL, 10000, 40));

    ASSERT_TRUE(engine.submit_order(2, Side::BUY, 10000, 100, fills));
    ASSERT_EQ(fills.size(), 2u);
    EXPECT_EQ(fills[0].quantity, 40);
    EXPECT_EQ(fills[1].quantity, 40);

    ASSERT_EQ(book.order_count(), 1u);
    ASSERT_TRUE(book.best_bid().has_value());
    EXPECT_EQ(*book.best_bid(), 10000);
    const auto snap = book.snapshot();
    ASSERT_EQ(snap.bids.size(), 1u);
    ASSERT_EQ(snap.bids[0].orders.size(), 1u);
    EXPECT_EQ(snap.bids[0].orders[0].order_id, 2u);
    EXPECT_EQ(snap.bids[0].orders[0].quantity, 60);
}

TEST(MatchingEngine, NoMatchRestsEntireOrder) {
    LimitOrderBook book;
    MatchingEngine engine(book);
    std::vector<FillData> fills;

    ASSERT_TRUE(book.add_order(1, Side::SELL, 10100, 50));

    ASSERT_TRUE(engine.submit_order(2, Side::BUY, 10000, 10, fills));
    EXPECT_TRUE(fills.empty());

    ASSERT_EQ(book.order_count(), 2u);
    ASSERT_TRUE(book.best_bid().has_value());
    EXPECT_EQ(*book.best_bid(), 10000);
    ASSERT_TRUE(book.best_ask().has_value());
    EXPECT_EQ(*book.best_ask(), 10100);
}

TEST(MatchingEngine, MultipleFillsPriceTimePriority) {
    LimitOrderBook book;
    MatchingEngine engine(book);
    std::vector<FillData> fills;

    ASSERT_TRUE(book.add_order(10, Side::SELL, 10000, 10));
    ASSERT_TRUE(book.add_order(11, Side::SELL, 10000, 20));
    ASSERT_TRUE(book.add_order(12, Side::SELL, 10100, 100));

    ASSERT_TRUE(engine.submit_order(20, Side::BUY, 10100, 35, fills));

    ASSERT_EQ(fills.size(), 6u);

    EXPECT_EQ(fills[0].order_id, 10u);
    EXPECT_EQ(fills[0].quantity, 10);
    EXPECT_EQ(fills[1].order_id, 20u);
    EXPECT_EQ(fills[1].quantity, 10);

    EXPECT_EQ(fills[2].order_id, 11u);
    EXPECT_EQ(fills[2].quantity, 20);
    EXPECT_EQ(fills[3].order_id, 20u);
    EXPECT_EQ(fills[3].quantity, 20);

    EXPECT_EQ(fills[4].order_id, 12u);
    EXPECT_EQ(fills[4].quantity, 5);
    EXPECT_EQ(fills[5].order_id, 20u);
    EXPECT_EQ(fills[5].quantity, 5);

    ASSERT_EQ(book.order_count(), 1u);
    const auto snap = book.snapshot();
    ASSERT_EQ(snap.asks.size(), 1u);
    EXPECT_EQ(snap.asks[0].orders[0].order_id, 12u);
    EXPECT_EQ(snap.asks[0].orders[0].quantity, 95);
}

TEST(MatchingEngine, SellMatchesBidLevelsFifo) {
    LimitOrderBook book;
    MatchingEngine engine(book);
    std::vector<FillData> fills;

    ASSERT_TRUE(book.add_order(1, Side::BUY, 10000, 5));
    ASSERT_TRUE(book.add_order(2, Side::BUY, 10000, 5));
    ASSERT_TRUE(engine.submit_order(3, Side::SELL, 10000, 7, fills));

    ASSERT_EQ(fills.size(), 4u);
    EXPECT_EQ(fills[0].order_id, 1u);
    EXPECT_EQ(fills[0].quantity, 5);
    EXPECT_EQ(fills[2].order_id, 2u);
    EXPECT_EQ(fills[2].quantity, 2);

    ASSERT_EQ(book.order_count(), 1u);
    const auto snap = book.snapshot();
    ASSERT_EQ(snap.bids.size(), 1u);
    EXPECT_EQ(snap.bids[0].orders[0].order_id, 2u);
    EXPECT_EQ(snap.bids[0].orders[0].quantity, 3);
}

TEST(MatchingEngine, DuplicateAggressorIdRejected) {
    LimitOrderBook book;
    MatchingEngine engine(book);
    std::vector<FillData> fills;

    ASSERT_TRUE(book.add_order(1, Side::SELL, 10000, 10));
    ASSERT_TRUE(book.add_order(2, Side::BUY, 10000, 1));

    EXPECT_FALSE(engine.submit_order(2, Side::BUY, 10000, 5, fills));
}

TEST(MatchingEngine, SelfTradePreventionSameStrategy) {
    LimitOrderBook book;
    MatchingEngine engine(book);
    std::vector<FillData> fills;
    constexpr StrategyId kS = 7;

    ASSERT_TRUE(book.add_order(1, Side::SELL, 10000, 100, kS));
    ASSERT_TRUE(engine.submit_order(2, Side::BUY, 10000, 100, fills, kS));
    EXPECT_TRUE(fills.empty());
    EXPECT_EQ(book.order_count(), 2u);
}

TEST(MatchingEngine, SelfTradeDifferentStrategyMatches) {
    LimitOrderBook book;
    MatchingEngine engine(book);
    std::vector<FillData> fills;

    ASSERT_TRUE(book.add_order(1, Side::SELL, 10000, 50, 7));
    ASSERT_TRUE(engine.submit_order(2, Side::BUY, 10000, 50, fills, 8));
    ASSERT_EQ(fills.size(), 2u);
    EXPECT_EQ(fills[0].quantity, 50);
    EXPECT_EQ(book.order_count(), 0u);
}

} // namespace
