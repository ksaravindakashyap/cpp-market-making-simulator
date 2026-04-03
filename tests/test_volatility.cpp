#include "mmsim/volatility.h"

#include <cmath>
#include <span>
#include <vector>

#include <gtest/gtest.h>

namespace {

using mmsim::OhlcBar;
using mmsim::Price;
using mmsim::VolatilityEstimator;

constexpr double kTol = 1e-9;

std::vector<Price> make_geometric_closes(Price start, double factor, std::size_t count) {
    std::vector<Price> out;
    out.reserve(count);
    double x = static_cast<double>(start);
    for (std::size_t i = 0; i < count; ++i) {
        out.push_back(static_cast<Price>(std::llround(x)));
        x *= factor;
    }
    return out;
}

TEST(VolatilityEstimator, CloseToCloseZeroOnFlat) {
    VolatilityEstimator est(5);
    std::vector<Price> closes(6, 10000);
    const auto v = est.close_to_close(closes);
    ASSERT_TRUE(v.has_value());
    EXPECT_NEAR(*v, 0.0, kTol);
}

TEST(VolatilityEstimator, CloseToCloseZeroOnConstantLogReturn) {
    VolatilityEstimator est(2);
    // Exact ratio 1.01 between consecutive closes -> identical log returns, zero sample stdev.
    std::vector<Price> closes = {10000, 10100, 10201};
    const auto v = est.close_to_close(closes);
    ASSERT_TRUE(v.has_value());
    EXPECT_NEAR(*v, 0.0, kTol);
}

TEST(VolatilityEstimator, CloseToCloseMatchesManualTwoReturns) {
    VolatilityEstimator est(2);
    std::vector<Price> closes = {10000, 10100, 10000};
    const auto v = est.close_to_close(closes);
    ASSERT_TRUE(v.has_value());

    const double r0 = std::log(10100.0 / 10000.0);
    const double r1 = std::log(10000.0 / 10100.0);
    const double mean = (r0 + r1) / 2.0;
    const double var = ((r0 - mean) * (r0 - mean) + (r1 - mean) * (r1 - mean)) / 1.0;
    const double expected = std::sqrt(var);
    EXPECT_NEAR(*v, expected, 1e-12);
}

TEST(VolatilityEstimator, CloseToCloseInsufficientData) {
    VolatilityEstimator est(5);
    std::vector<Price> closes = {10000, 10100};
    EXPECT_FALSE(est.close_to_close(closes).has_value());
}

TEST(VolatilityEstimator, ParkinsonZeroWhenHighEqualsLow) {
    VolatilityEstimator est(3);
    std::vector<OhlcBar> bars;
    for (int i = 0; i < 3; ++i) {
        bars.push_back(OhlcBar{10000, 10000, 10000, 10000});
    }
    const auto v = est.parkinson(bars);
    ASSERT_TRUE(v.has_value());
    EXPECT_NEAR(*v, 0.0, kTol);
}

TEST(VolatilityEstimator, ParkinsonMatchesFormulaSingleBarWindow) {
    VolatilityEstimator est(1);
    OhlcBar b{};
    b.open = 10000;
    b.close = 10000;
    b.high = 20000;
    b.low_price = 10000;
    std::vector<OhlcBar> bars = {b};

    const auto v = est.parkinson(bars);
    ASSERT_TRUE(v.has_value());

    const double hl = std::log(2.0);
    const double inv = 1.0 / (4.0 * std::log(2.0));
    const double expected = std::sqrt(inv * hl * hl);
    EXPECT_NEAR(*v, expected, 1e-12);
}

TEST(VolatilityEstimator, ParkinsonAverageOverWindow) {
    VolatilityEstimator est(2);
    OhlcBar a{};
    a.open = a.close = a.high = a.low_price = 10000;
    OhlcBar b = a;
    b.high = 20000;
    b.low_price = 10000;
    std::vector<OhlcBar> bars = {a, b};

    const auto v = est.parkinson(bars);
    ASSERT_TRUE(v.has_value());

    const double hl0 = 0.0;
    const double hl1 = std::log(2.0);
    const double inv = 1.0 / (4.0 * std::log(2.0));
    const double mean_sq = (hl0 * hl0 + hl1 * hl1) / 2.0;
    const double expected = std::sqrt(inv * mean_sq);
    EXPECT_NEAR(*v, expected, 1e-12);
}

TEST(VolatilityEstimator, YangZhangZeroOnFlatOhlc) {
    VolatilityEstimator est(3);
    std::vector<OhlcBar> hist;
    hist.push_back(OhlcBar{10000, 10000, 10000, 10000});
    for (int i = 0; i < 3; ++i) {
        hist.push_back(OhlcBar{10000, 10000, 10000, 10000});
    }
    const auto v = est.yang_zhang(hist);
    ASSERT_TRUE(v.has_value());
    EXPECT_NEAR(*v, 0.0, kTol);
}

TEST(VolatilityEstimator, YangZhangAndParkinsonPositiveOnSameSyntheticRange) {
    VolatilityEstimator est(5);
    std::vector<OhlcBar> hist;
    hist.push_back(OhlcBar{10000, 10000, 10000, 10000});
    for (int i = 0; i < 5; ++i) {
        OhlcBar b{};
        b.open = 10000;
        b.close = 10000;
        b.high = 11000;
        b.low_price = 9000;
        hist.push_back(b);
    }

    const auto p = est.parkinson(std::span<const OhlcBar>(hist.data() + 1, 5));
    const auto yz = est.yang_zhang(hist);
    ASSERT_TRUE(p.has_value());
    ASSERT_TRUE(yz.has_value());
    EXPECT_GT(*p, 0.0);
    EXPECT_GT(*yz, 0.0);
    EXPECT_NE(*p, *yz);
}

TEST(VolatilityEstimator, EstimatorsOrderedOnTrendingSynthetic) {
    VolatilityEstimator est(10);
    std::vector<Price> closes = make_geometric_closes(10000, 1.002, 20);
    std::vector<OhlcBar> hist;
    hist.reserve(closes.size());
    for (Price c : closes) {
        OhlcBar b{};
        b.open = c;
        b.close = c;
        b.high = static_cast<Price>(static_cast<double>(c) * 1.001);
        b.low_price = static_cast<Price>(static_cast<double>(c) * 0.999);
        hist.push_back(b);
    }

    const auto c2c = est.close_to_close(closes);
    const auto p = est.parkinson(hist);
    const auto yz = est.yang_zhang(hist);
    ASSERT_TRUE(c2c.has_value());
    ASSERT_TRUE(p.has_value());
    ASSERT_TRUE(yz.has_value());
    EXPECT_GT(*p, *c2c);
    EXPECT_GT(*yz, *c2c);
}

TEST(VolatilityEstimator, WindowChange) {
    VolatilityEstimator est(3);
    std::vector<Price> closes = {10000, 10100, 10000, 10100, 10000};
    const auto v3 = est.close_to_close(closes);
    est.set_window(2);
    const auto v2 = est.close_to_close(closes);
    ASSERT_TRUE(v3.has_value());
    ASSERT_TRUE(v2.has_value());
    EXPECT_NE(*v3, *v2);
}

} // namespace
