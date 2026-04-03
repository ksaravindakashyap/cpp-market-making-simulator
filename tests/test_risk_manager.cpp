#include "mmsim/risk_manager.h"

#include <cmath>

#include <gtest/gtest.h>

namespace {

using mmsim::FillData;
using mmsim::RiskManager;
using mmsim::Side;

TEST(RiskManager, RealizedPnlRoundTripLong) {
    RiskManager::Config cfg;
    cfg.max_position = 1000;
    RiskManager rm(cfg);

    rm.on_fill(FillData{1, 10000, 10, 0}, Side::BUY);
    rm.on_fill(FillData{2, 11000, 10, 0}, Side::SELL);
    rm.mark(11000);

    EXPECT_EQ(rm.position(), 0);
    EXPECT_NEAR(rm.realized_pnl(), 10.0 * (11000.0 - 10000.0), 1e-6);
    EXPECT_NEAR(rm.unrealized_pnl(), 0.0, 1e-6);
}

TEST(RiskManager, UnrealizedMarkToMarket) {
    RiskManager::Config cfg;
    cfg.max_position = 1000;
    RiskManager rm(cfg);

    rm.on_fill(FillData{1, 10000, 5, 0}, Side::BUY);
    rm.mark(10000);
    EXPECT_NEAR(rm.unrealized_pnl(), 0.0, 1e-6);

    rm.mark(10200);
    EXPECT_NEAR(rm.unrealized_pnl(), 5.0 * 200.0, 1e-6);
    EXPECT_NEAR(rm.equity(), rm.realized_pnl() + rm.unrealized_pnl(), 1e-6);
}

TEST(RiskManager, PositionLimitRejectsBreaches) {
    RiskManager::Config cfg;
    cfg.max_position = 100;
    RiskManager rm(cfg);

    EXPECT_TRUE(rm.allow_order(Side::BUY, 100));
    EXPECT_FALSE(rm.allow_order(Side::BUY, 101));
    EXPECT_TRUE(rm.allow_order(Side::SELL, 50));

    rm.on_fill(FillData{1, 10000, 80, 0}, Side::BUY);
    EXPECT_TRUE(rm.allow_order(Side::BUY, 20));
    EXPECT_FALSE(rm.allow_order(Side::BUY, 21));
}

TEST(RiskManager, MaxDrawdownTracksPeakEquity) {
    RiskManager::Config cfg;
    RiskManager rm(cfg);

    rm.on_fill(FillData{1, 10000, 1, 0}, Side::BUY);
    rm.mark(10000);
    rm.mark(20000);
    rm.mark(10000);
    EXPECT_GT(rm.max_drawdown(), 0.0);
    EXPECT_NEAR(rm.max_drawdown(), 10000.0, 1.0);
}

TEST(RiskManager, SharpePositiveWhenReturnsTrendUp) {
    RiskManager::Config cfg;
    cfg.min_periods_for_risk_metrics = 2;
    cfg.periods_per_year = 252.0;
    RiskManager rm(cfg);

    rm.on_fill(FillData{1, 10000, 1, 0}, Side::BUY);
    mmsim::Price px = 10000;
    for (int i = 0; i < 6; ++i) {
        px += 100;
        rm.mark(px);
    }

    const auto sharpe = rm.sharpe_ratio();
    ASSERT_TRUE(sharpe.has_value());
    EXPECT_GT(*sharpe, 0.0);
}

TEST(RiskManager, SortinoDefinedWhenDownsideReturnsExist) {
    RiskManager::Config cfg;
    cfg.min_periods_for_risk_metrics = 2;
    cfg.periods_per_year = 252.0;
    RiskManager rm(cfg);

    rm.on_fill(FillData{1, 10000, 1, 0}, Side::BUY);
    rm.mark(10000);
    rm.mark(12000);
    rm.mark(9000);
    rm.mark(10000);

    const auto sortino = rm.sortino_ratio();
    ASSERT_TRUE(sortino.has_value());
    EXPECT_FALSE(std::isnan(*sortino));
}

TEST(RiskManager, FillRateAndSpreadCapture) {
    RiskManager::Config cfg;
    RiskManager rm(cfg);

    rm.on_order_submitted();
    rm.on_order_submitted();
    rm.on_fill(FillData{1, 9900, 1, 0}, Side::BUY, 10000);
    rm.on_fill(FillData{2, 10100, 1, 0}, Side::SELL, 10000);

    EXPECT_NEAR(rm.fill_rate(), 1.0, 1e-9);
    EXPECT_NEAR(rm.average_spread_captured(), 100.0, 1e-9);
}

TEST(RiskManager, InventoryStatistics) {
    RiskManager::Config cfg;
    RiskManager rm(cfg);

    rm.on_fill(FillData{1, 10000, 3, 0}, Side::BUY);
    rm.mark(10000);
    rm.on_fill(FillData{2, 10000, 1, 0}, Side::SELL);
    rm.mark(10000);

    EXPECT_GT(rm.mean_inventory(), 0.0);
    EXPECT_GE(rm.max_abs_inventory_observed(), 3);
    EXPECT_GE(rm.variance_inventory(), 0.0);
}

TEST(RiskManager, FeeReducesRealizedPnl) {
    RiskManager::Config cfg;
    RiskManager rm(cfg);
    rm.on_fill(FillData{1, 10000, 10, 100}, Side::BUY);
    rm.mark(10000);
    EXPECT_NEAR(rm.realized_pnl(), -100.0, 1e-6);
}

} // namespace
