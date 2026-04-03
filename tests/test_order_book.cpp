#include "mmsim/order_book.h"

#include <gtest/gtest.h>

namespace {

using mmsim::LimitOrderBook;
using mmsim::Price;
using mmsim::Side;

TEST(LimitOrderBook, AddBidAskBestAndMid) {
    LimitOrderBook book;
    EXPECT_TRUE(book.add_order(1, Side::BUY, 10000, 5));
    EXPECT_TRUE(book.add_order(2, Side::SELL, 10100, 3));

    ASSERT_TRUE(book.best_bid().has_value());
    EXPECT_EQ(*book.best_bid(), 10000);
    ASSERT_TRUE(book.best_ask().has_value());
    EXPECT_EQ(*book.best_ask(), 10100);

    ASSERT_TRUE(book.mid_price().has_value());
    EXPECT_EQ(*book.mid_price(), (10000 + 10100) / 2);
}

TEST(LimitOrderBook, BestBidIsHighest) {
    LimitOrderBook book;
    EXPECT_TRUE(book.add_order(1, Side::BUY, 10000, 1));
    EXPECT_TRUE(book.add_order(2, Side::BUY, 10100, 1));
    EXPECT_TRUE(book.add_order(3, Side::BUY, 9990, 1));
    ASSERT_TRUE(book.best_bid().has_value());
    EXPECT_EQ(*book.best_bid(), 10100);
}

TEST(LimitOrderBook, BestAskIsLowest) {
    LimitOrderBook book;
    EXPECT_TRUE(book.add_order(1, Side::SELL, 10200, 1));
    EXPECT_TRUE(book.add_order(2, Side::SELL, 10100, 1));
    EXPECT_TRUE(book.add_order(3, Side::SELL, 10300, 1));
    ASSERT_TRUE(book.best_ask().has_value());
    EXPECT_EQ(*book.best_ask(), 10100);
}

TEST(LimitOrderBook, MidPriceNulloptWhenOneSideMissing) {
    LimitOrderBook book;
    EXPECT_TRUE(book.add_order(1, Side::BUY, 10000, 1));
    EXPECT_FALSE(book.mid_price().has_value());

    LimitOrderBook book2;
    EXPECT_TRUE(book2.add_order(1, Side::SELL, 10000, 1));
    EXPECT_FALSE(book2.mid_price().has_value());
}

TEST(LimitOrderBook, CancelRemovesOrderAndUpdatesBests) {
    LimitOrderBook book;
    EXPECT_TRUE(book.add_order(10, Side::BUY, 10000, 1));
    EXPECT_TRUE(book.add_order(11, Side::BUY, 10100, 1));
    EXPECT_TRUE(book.add_order(20, Side::SELL, 10200, 1));

    EXPECT_TRUE(book.cancel_order(11));
    ASSERT_TRUE(book.best_bid().has_value());
    EXPECT_EQ(*book.best_bid(), 10000);
    EXPECT_EQ(book.order_count(), 2u);

    EXPECT_TRUE(book.cancel_order(20));
    EXPECT_FALSE(book.best_ask().has_value());
    EXPECT_EQ(book.order_count(), 1u);
}

TEST(LimitOrderBook, CancelLastOrderAtLevelRemovesLevel) {
    LimitOrderBook book;
    EXPECT_TRUE(book.add_order(1, Side::BUY, 10000, 5));
    EXPECT_TRUE(book.cancel_order(1));
    EXPECT_FALSE(book.best_bid().has_value());
    EXPECT_EQ(book.order_count(), 0u);

    const auto snap = book.snapshot();
    EXPECT_TRUE(snap.bids.empty());
}

TEST(LimitOrderBook, DuplicateOrderIdRejected) {
    LimitOrderBook book;
    EXPECT_TRUE(book.add_order(1, Side::BUY, 10000, 1));
    EXPECT_FALSE(book.add_order(1, Side::SELL, 10100, 1));
    EXPECT_EQ(book.order_count(), 1u);
}

TEST(LimitOrderBook, NonPositiveQuantityRejected) {
    LimitOrderBook book;
    EXPECT_FALSE(book.add_order(1, Side::BUY, 10000, 0));
    EXPECT_FALSE(book.add_order(2, Side::BUY, 10000, -1));
    EXPECT_EQ(book.order_count(), 0u);
}

TEST(LimitOrderBook, CancelUnknownReturnsFalse) {
    LimitOrderBook book;
    EXPECT_FALSE(book.cancel_order(999));
}

TEST(LimitOrderBook, SnapshotOrderingAndFifo) {
    LimitOrderBook book;
    EXPECT_TRUE(book.add_order(1, Side::BUY, 10000, 10));
    EXPECT_TRUE(book.add_order(2, Side::BUY, 10000, 20));
    EXPECT_TRUE(book.add_order(3, Side::BUY, 10100, 5));
    EXPECT_TRUE(book.add_order(4, Side::SELL, 10100, 7));
    EXPECT_TRUE(book.add_order(5, Side::SELL, 10100, 8));
    EXPECT_TRUE(book.add_order(6, Side::SELL, 10200, 1));

    const auto snap = book.snapshot();

    ASSERT_EQ(snap.bids.size(), 2u);
    EXPECT_EQ(snap.bids[0].price, 10100);
    ASSERT_EQ(snap.bids[0].orders.size(), 1u);
    EXPECT_EQ(snap.bids[0].orders[0].order_id, 3u);
    EXPECT_EQ(snap.bids[0].orders[0].quantity, 5);

    EXPECT_EQ(snap.bids[1].price, 10000);
    ASSERT_EQ(snap.bids[1].orders.size(), 2u);
    EXPECT_EQ(snap.bids[1].orders[0].order_id, 1u);
    EXPECT_EQ(snap.bids[1].orders[0].quantity, 10);
    EXPECT_EQ(snap.bids[1].orders[1].order_id, 2u);
    EXPECT_EQ(snap.bids[1].orders[1].quantity, 20);

    ASSERT_EQ(snap.asks.size(), 2u);
    EXPECT_EQ(snap.asks[0].price, 10100);
    ASSERT_EQ(snap.asks[0].orders.size(), 2u);
    EXPECT_EQ(snap.asks[0].orders[0].order_id, 4u);
    EXPECT_EQ(snap.asks[0].orders[1].order_id, 5u);

    EXPECT_EQ(snap.asks[1].price, 10200);
    ASSERT_EQ(snap.asks[1].orders.size(), 1u);
    EXPECT_EQ(snap.asks[1].orders[0].order_id, 6u);
}

TEST(LimitOrderBook, SnapshotMultipleBidLevelsDescending) {
    LimitOrderBook book;
    EXPECT_TRUE(book.add_order(1, Side::BUY, 10000, 1));
    EXPECT_TRUE(book.add_order(2, Side::BUY, 10000, 1));
    EXPECT_TRUE(book.add_order(3, Side::BUY, 9990, 1));
    EXPECT_TRUE(book.add_order(4, Side::SELL, 10100, 1));

    const auto snap = book.snapshot();
    ASSERT_EQ(snap.bids.size(), 2u);
    EXPECT_EQ(snap.bids[0].price, 10000);
    ASSERT_EQ(snap.bids[0].orders.size(), 2u);
    EXPECT_EQ(snap.bids[1].price, 9990);
}

TEST(LimitOrderBook, OrderCountMatchesIndex) {
    LimitOrderBook book;
    for (std::uint64_t i = 1; i <= 50; ++i) {
        EXPECT_TRUE(book.add_order(i, Side::BUY, 10000 + static_cast<Price>(i), 1));
    }
    EXPECT_EQ(book.order_count(), 50u);
    std::size_t n = 0;
    for (const auto& lvl : book.snapshot().bids) {
        n += lvl.orders.size();
    }
    EXPECT_EQ(n, 50u);
}

TEST(LimitOrderBook, EmptyBookNoBests) {
    const LimitOrderBook book;
    EXPECT_FALSE(book.best_bid().has_value());
    EXPECT_FALSE(book.best_ask().has_value());
    EXPECT_FALSE(book.mid_price().has_value());
    const auto snap = book.snapshot();
    EXPECT_TRUE(snap.bids.empty());
    EXPECT_TRUE(snap.asks.empty());
    EXPECT_EQ(book.order_count(), 0u);
}

TEST(LimitOrderBook, CancelMiddleOrderInLevelPreservesFifo) {
    LimitOrderBook book;
    EXPECT_TRUE(book.add_order(1, Side::BUY, 10000, 1));
    EXPECT_TRUE(book.add_order(2, Side::BUY, 10000, 2));
    EXPECT_TRUE(book.add_order(3, Side::BUY, 10000, 3));
    EXPECT_TRUE(book.cancel_order(2));

    const auto snap = book.snapshot();
    ASSERT_EQ(snap.bids.size(), 1u);
    ASSERT_EQ(snap.bids[0].orders.size(), 2u);
    EXPECT_EQ(snap.bids[0].orders[0].order_id, 1u);
    EXPECT_EQ(snap.bids[0].orders[1].order_id, 3u);
}

} // namespace
