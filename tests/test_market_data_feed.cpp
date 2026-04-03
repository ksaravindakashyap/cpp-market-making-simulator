#include "mmsim/market_data_feed.h"
#include "mmsim/matching_engine.h"
#include "mmsim/order_book.h"

#include <chrono>
#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

namespace {

using mmsim::Event;
using mmsim::EventBus;
using mmsim::EventType;
using mmsim::LimitOrderBook;
using mmsim::MarketDataFeed;
using mmsim::MatchingEngine;
using mmsim::ReplaySpeed;
using mmsim::Side;
using mmsim::SimulatedMarketOrderFlow;

std::filesystem::path make_temp_csv(const char* content) {
    const auto p = std::filesystem::temp_directory_path() / "mmsim_ticks_test.csv";
    std::ofstream o(p);
    o << content;
    return p;
}

TEST(MarketDataFeed, LoadCsvAndPublishMarketTick) {
    const auto path = make_temp_csv("timestamp_ns,price,quantity,side\n"
                                    "1000,10000,1,BUY\n"
                                    "2000,10100,2,SELL\n");

    MarketDataFeed feed;
    ASSERT_TRUE(feed.load_csv(path.string()));
    EXPECT_EQ(feed.tick_count(), 2u);

    EventBus bus(16);
    feed.set_replay_speed(ReplaySpeed::Max);
    ASSERT_TRUE(feed.try_push_next(bus));
    ASSERT_TRUE(feed.try_push_next(bus));

    Event a{};
    Event b{};
    ASSERT_TRUE(bus.try_pop(a));
    ASSERT_TRUE(bus.try_pop(b));
    EXPECT_EQ(a.type, EventType::MarketTick);
    EXPECT_EQ(b.type, EventType::MarketTick);
    EXPECT_EQ(a.market_tick.timestamp_ns, 1000u);
    EXPECT_EQ(a.market_tick.price, 10000);
    EXPECT_EQ(a.market_tick.quantity, 1);
    EXPECT_EQ(a.market_tick.side, Side::BUY);
}

TEST(MarketDataFeed, ReplaySpeedMaxIsFast) {
    const auto path = make_temp_csv("1000000000,10000,1,BUY\n"
                                    "2000000000,10000,1,BUY\n");

    MarketDataFeed feed;
    ASSERT_TRUE(feed.load_csv(path.string()));
    feed.set_replay_speed(ReplaySpeed::Max);
    EventBus bus(8);

    const auto t0 = std::chrono::steady_clock::now();
    ASSERT_TRUE(feed.try_push_next(bus));
    ASSERT_TRUE(feed.try_push_next(bus));
    const auto t1 = std::chrono::steady_clock::now();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    EXPECT_LT(ms, 50);
}

TEST(SimulatedMarketOrderFlow, MarketBuyHitsRestingAsk) {
    LimitOrderBook book;
    MatchingEngine engine(book);
    SimulatedMarketOrderFlow flow(engine);

    ASSERT_TRUE(book.add_order(1, Side::SELL, 10100, 10));

    mmsim::MarketTickData tick{};
    tick.timestamp_ns = 1;
    tick.price = 10100;
    tick.quantity = 4;
    tick.side = Side::BUY;

    std::vector<mmsim::FillData> fills;
    ASSERT_TRUE(flow.execute_tick(tick, 100, fills));
    EXPECT_FALSE(fills.empty());
    EXPECT_EQ(book.order_count(), 1u);
}

TEST(SimulatedMarketOrderFlow, MarketSellHitsRestingBid) {
    LimitOrderBook book;
    MatchingEngine engine(book);
    SimulatedMarketOrderFlow flow(engine);

    ASSERT_TRUE(book.add_order(1, Side::BUY, 9900, 5));

    mmsim::MarketTickData tick{};
    tick.timestamp_ns = 1;
    tick.price = 9900;
    tick.quantity = 2;
    tick.side = Side::SELL;

    std::vector<mmsim::FillData> fills;
    ASSERT_TRUE(flow.execute_tick(tick, 200, fills));
    EXPECT_FALSE(fills.empty());
}

} // namespace
