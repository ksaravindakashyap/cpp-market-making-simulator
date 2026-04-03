#include "mmsim/volatility.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace mmsim {

namespace {

constexpr double kParkinsonInv = 1.0 / (4.0 * std::log(2.0));

bool positive(Price p) {
    return p > 0;
}

} // namespace

VolatilityEstimator::VolatilityEstimator(std::size_t window) : window_{window} {}

std::optional<double> VolatilityEstimator::close_to_close(std::span<const Price> closes) const {
    if (window_ < 1 || closes.size() < window_ + 1) {
        return std::nullopt;
    }

    const std::size_t start = closes.size() - window_ - 1;
    double sum = 0.0;
    std::vector<double> rets;
    rets.reserve(window_);
    for (std::size_t i = 0; i < window_; ++i) {
        const Price a = closes[start + i];
        const Price b = closes[start + i + 1];
        if (!positive(a) || !positive(b)) {
            return std::nullopt;
        }
        const double r = std::log(static_cast<double>(b) / static_cast<double>(a));
        rets.push_back(r);
        sum += r;
    }

    if (window_ == 1) {
        return 0.0;
    }

    const double mean = sum / static_cast<double>(window_);
    double acc = 0.0;
    for (double r : rets) {
        const double d = r - mean;
        acc += d * d;
    }
    const double sample_var = acc / static_cast<double>(window_ - 1);
    return std::sqrt(sample_var);
}

std::optional<double> VolatilityEstimator::parkinson(std::span<const OhlcBar> bars) const {
    if (window_ < 1 || bars.size() < window_) {
        return std::nullopt;
    }

    const std::size_t start = bars.size() - window_;
    double sum_log_range_sq = 0.0;
    for (std::size_t i = start; i < bars.size(); ++i) {
        const OhlcBar& b = bars[i];
        if (!positive(b.high) || !positive(b.low_price) || b.high < b.low_price) {
            return std::nullopt;
        }
        const double hl = std::log(static_cast<double>(b.high) / static_cast<double>(b.low_price));
        sum_log_range_sq += hl * hl;
    }

    const double mean_sq = sum_log_range_sq / static_cast<double>(window_);
    const double variance = kParkinsonInv * mean_sq;
    return std::sqrt(std::max(0.0, variance));
}

std::optional<double> VolatilityEstimator::yang_zhang(std::span<const OhlcBar> history) const {
    if (window_ < 2 || history.size() < window_ + 1) {
        return std::nullopt;
    }

    const std::size_t n = window_;
    const std::size_t start = history.size() - n;

    std::vector<double> u;
    u.reserve(n);
    std::vector<double> c;
    c.reserve(n);
    std::vector<double> rs;
    rs.reserve(n);

    for (std::size_t i = 0; i < n; ++i) {
        const OhlcBar& bar = history[start + i];
        const Price prev_close = (i == 0) ? history[start - 1].close : history[start + i - 1].close;

        if (!positive(bar.open) || !positive(bar.close) || !positive(bar.high) ||
            !positive(bar.low_price) || !positive(prev_close)) {
            return std::nullopt;
        }
        if (bar.high < bar.low_price) {
            return std::nullopt;
        }

        u.push_back(std::log(static_cast<double>(bar.open) / static_cast<double>(prev_close)));
        c.push_back(std::log(static_cast<double>(bar.close) / static_cast<double>(bar.open)));

        const double hc = std::log(static_cast<double>(bar.high) / static_cast<double>(bar.close));
        const double ho = std::log(static_cast<double>(bar.high) / static_cast<double>(bar.open));
        const double lc =
            std::log(static_cast<double>(bar.low_price) / static_cast<double>(bar.close));
        const double lo =
            std::log(static_cast<double>(bar.low_price) / static_cast<double>(bar.open));
        rs.push_back(hc * ho + lc * lo);
    }

    double mean_u = 0.0;
    double mean_c = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        mean_u += u[i];
        mean_c += c[i];
    }
    mean_u /= static_cast<double>(n);
    mean_c /= static_cast<double>(n);

    double var_u = 0.0;
    double var_c = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double du = u[i] - mean_u;
        const double dc = c[i] - mean_c;
        var_u += du * du;
        var_c += dc * dc;
    }
    var_u /= static_cast<double>(n - 1);
    var_c /= static_cast<double>(n - 1);

    double mean_rs = 0.0;
    for (double x : rs) {
        mean_rs += x;
    }
    mean_rs /= static_cast<double>(n);

    const double nd = static_cast<double>(n);
    const double k = 0.34 / (1.34 + (nd + 1.0) / (nd - 1.0));

    const double var_yz = var_u + k * var_c + (1.0 - k) * mean_rs;
    return std::sqrt(std::max(0.0, var_yz));
}

} // namespace mmsim
