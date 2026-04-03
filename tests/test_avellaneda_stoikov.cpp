#include "mmsim/avellaneda_stoikov.h"

#include <cmath>
#include <gtest/gtest.h>

namespace {

using mmsim::AvellanedaStoikovStrategy;
using mmsim::Price;
using mmsim::Quantity;

AvellanedaStoikovStrategy make_strategy() {
    AvellanedaStoikovStrategy::Config cfg;
    cfg.gamma = 1.0;
    cfg.kappa = 1.0;
    return AvellanedaStoikovStrategy{cfg};
}

TEST(AvellanedaStoikov, QuoteSymmetryAtZeroInventory) {
    const auto s = make_strategy();
    const Price mid = 100000;
    const double sigma = 0.2;
    const double tau = 1.0;

    const auto q = s.optimal_quotes(mid, 0, sigma, tau);
    ASSERT_TRUE(q.has_value());

    const double half_spread_up = static_cast<double>(q->ask) - static_cast<double>(mid);
    const double half_spread_down = static_cast<double>(mid) - static_cast<double>(q->bid);
    EXPECT_NEAR(half_spread_up, half_spread_down, 1.0);

    const auto r = s.reservation_price(mid, 0, sigma, tau);
    const double center = 0.5 * (static_cast<double>(q->bid) + static_cast<double>(q->ask));
    EXPECT_NEAR(center, r, 1.0);
}

TEST(AvellanedaStoikov, QuoteSkewLongInventoryShiftsDownVersusFlat) {
    const auto s = make_strategy();
    const Price mid = 100000;
    const double sigma = 0.5;
    const double tau = 1.0;

    const auto flat = s.optimal_quotes(mid, 0, sigma, tau);
    const auto long_inv = s.optimal_quotes(mid, 200, sigma, tau);
    ASSERT_TRUE(flat.has_value());
    ASSERT_TRUE(long_inv.has_value());

    EXPECT_LT(long_inv->bid, flat->bid);
    EXPECT_LT(long_inv->ask, flat->ask);

    const double r0 = s.reservation_price(mid, 0, sigma, tau);
    const double r1 = s.reservation_price(mid, 200, sigma, tau);
    EXPECT_LT(r1, r0);
}

TEST(AvellanedaStoikov, QuoteSkewShortInventoryShiftsUpVersusFlat) {
    const auto s = make_strategy();
    const Price mid = 100000;
    const double sigma = 0.5;
    const double tau = 1.0;

    const auto flat = s.optimal_quotes(mid, 0, sigma, tau);
    const auto sh = s.optimal_quotes(mid, -200, sigma, tau);
    ASSERT_TRUE(flat.has_value());
    ASSERT_TRUE(sh.has_value());

    EXPECT_GT(sh->bid, flat->bid);
    EXPECT_GT(sh->ask, flat->ask);

    const double r0 = s.reservation_price(mid, 0, sigma, tau);
    const double r1 = s.reservation_price(mid, -200, sigma, tau);
    EXPECT_GT(r1, r0);
}

TEST(AvellanedaStoikov, WiderSpreadWithHigherVolatility) {
    const auto s = make_strategy();
    const Price mid = 100000;
    const double tau = 1.0;

    // Large sigma separation so integer bid/ask width reflects wider spread after rounding.
    const auto low = s.optimal_quotes(mid, 0, 0.05, tau);
    const auto high = s.optimal_quotes(mid, 0, 3.0, tau);
    ASSERT_TRUE(low.has_value());
    ASSERT_TRUE(high.has_value());

    const double w_low = static_cast<double>(low->ask - low->bid);
    const double w_high = static_cast<double>(high->ask - high->bid);
    EXPECT_GT(w_high, w_low);
    EXPECT_GT(s.optimal_spread(3.0, tau), s.optimal_spread(0.05, tau));
}

TEST(AvellanedaStoikov, OnTickInvokesCallbackWithQuotes) {
    const auto s = make_strategy();
    Price got_bid = 0;
    Price got_ask = 0;
    int calls = 0;

    s.on_tick(100000, 0, 0.2, 1.0, [&](Price bid, Price ask) {
        got_bid = bid;
        got_ask = ask;
        ++calls;
    });

    EXPECT_EQ(calls, 1);
    EXPECT_LT(got_bid, got_ask);
    const auto expected = s.optimal_quotes(100000, 0, 0.2, 1.0);
    ASSERT_TRUE(expected.has_value());
    EXPECT_EQ(got_bid, expected->bid);
    EXPECT_EQ(got_ask, expected->ask);
}

TEST(AvellanedaStoikov, InvalidConfigYieldsNoQuotes) {
    AvellanedaStoikovStrategy::Config bad;
    bad.gamma = -1.0;
    bad.kappa = 1.0;
    AvellanedaStoikovStrategy s(bad);
    EXPECT_FALSE(s.optimal_quotes(100000, 0, 0.02, 1.0).has_value());
}

} // namespace
