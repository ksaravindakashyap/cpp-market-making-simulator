#include "mmsim/risk_manager.h"

#include <algorithm>
#include <cmath>

namespace mmsim {

RiskManager::RiskManager(Config config) : cfg_(config) {}

void RiskManager::reset() noexcept {
    position_ = 0;
    avg_price_ = 0.0;
    realized_pnl_ = 0.0;
    unrealized_pnl_ = 0.0;
    peak_equity_ = 0.0;
    max_drawdown_ = 0.0;
    last_equity_for_return_ = 0.0;
    have_last_equity_ = false;
    period_returns_.clear();
    orders_submitted_ = 0;
    fills_recorded_ = 0;
    spread_capture_sum_ = 0.0;
    spread_capture_samples_ = 0;
    inventory_sum_ = 0.0;
    inventory_sum_sq_ = 0.0;
    inventory_samples_ = 0;
    max_abs_inventory_ = 0;
}

bool RiskManager::allow_order(Side side, Quantity quantity) const {
    if (quantity <= 0) {
        return false;
    }
    if (cfg_.max_position == 0) {
        return true;
    }
    const long double delta = (side == Side::BUY) ? static_cast<long double>(quantity)
                                                  : -static_cast<long double>(quantity);
    const long double np = static_cast<long double>(position_) + delta;
    const long double lim = static_cast<long double>(cfg_.max_position);
    return std::fabsl(np) <= lim + 1e-12L;
}

void RiskManager::on_order_submitted() {
    ++orders_submitted_;
}

void RiskManager::on_fill(const FillData& fill, Side side, std::optional<Price> mid_at_fill) {
    const Quantity q = fill.quantity;
    if (q <= 0) {
        return;
    }
    const Price p = fill.price;
    const double fee = static_cast<double>(fill.fee);

    if (side == Side::BUY) {
        realized_pnl_ -= fee;
        const double pd = static_cast<double>(p);
        const double qd = static_cast<double>(q);

        if (position_ >= 0) {
            if (position_ == 0) {
                position_ = q;
                avg_price_ = pd;
            } else {
                const double new_pos = static_cast<double>(position_) + qd;
                avg_price_ = (avg_price_ * static_cast<double>(position_) + pd * qd) / new_pos;
                position_ += q;
            }
        } else {
            const Quantity short_sz = -position_;
            const Quantity cover = q < short_sz ? q : short_sz;
            const Quantity rem = q - cover;

            realized_pnl_ += (avg_price_ - pd) * static_cast<double>(cover);

            position_ += cover;

            if (rem > 0) {
                position_ = rem;
                avg_price_ = pd;
            }
        }
    } else {
        realized_pnl_ -= fee;
        const double pd = static_cast<double>(p);
        const double qd = static_cast<double>(q);

        if (position_ <= 0) {
            if (position_ == 0) {
                position_ = -q;
                avg_price_ = pd;
            } else {
                const double short_sz = static_cast<double>(-position_);
                const double new_short = short_sz + qd;
                avg_price_ = (avg_price_ * short_sz + pd * qd) / new_short;
                position_ -= q;
            }
        } else {
            const Quantity long_sz = position_;
            const Quantity sell = q < long_sz ? q : long_sz;
            const Quantity rem = q - sell;

            realized_pnl_ += (pd - avg_price_) * static_cast<double>(sell);

            position_ -= sell;

            if (rem > 0) {
                position_ = -rem;
                avg_price_ = pd;
            }
        }
    }

    ++fills_recorded_;

    if (mid_at_fill.has_value()) {
        const double mid = static_cast<double>(*mid_at_fill);
        const double fp = static_cast<double>(p);
        const double half_spread = (side == Side::BUY) ? (mid - fp) : (fp - mid);
        spread_capture_sum_ += half_spread;
        ++spread_capture_samples_;
    }

    const Quantity ai = position_ >= 0 ? position_ : -position_;
    if (ai > max_abs_inventory_) {
        max_abs_inventory_ = ai;
    }
    inventory_sum_ += static_cast<double>(position_);
    inventory_sum_sq_ += static_cast<double>(position_) * static_cast<double>(position_);
    ++inventory_samples_;
}

void RiskManager::recompute_unrealized(Price mark_price) {
    if (position_ == 0) {
        unrealized_pnl_ = 0.0;
        return;
    }
    const double md = static_cast<double>(mark_price);
    unrealized_pnl_ = static_cast<double>(position_) * (md - avg_price_);
}

void RiskManager::update_equity_stats(double eq) {
    if (!have_last_equity_) {
        last_equity_for_return_ = eq;
        have_last_equity_ = true;
        peak_equity_ = eq;
        return;
    }

    double ret = 0.0;
    if (std::abs(last_equity_for_return_) > 1e-12) {
        ret = (eq - last_equity_for_return_) / std::abs(last_equity_for_return_);
    } else {
        ret = eq - last_equity_for_return_;
    }
    period_returns_.push_back(ret);

    last_equity_for_return_ = eq;
    peak_equity_ = std::max(peak_equity_, eq);
    const double dd = peak_equity_ - eq;
    max_drawdown_ = std::max(max_drawdown_, dd);
}

void RiskManager::mark(Price mark_price) {
    recompute_unrealized(mark_price);
    const double eq = equity();
    update_equity_stats(eq);

    const Quantity ai = position_ >= 0 ? position_ : -position_;
    if (ai > max_abs_inventory_) {
        max_abs_inventory_ = ai;
    }
    inventory_sum_ += static_cast<double>(position_);
    inventory_sum_sq_ += static_cast<double>(position_) * static_cast<double>(position_);
    ++inventory_samples_;
}

std::optional<double> RiskManager::sharpe_ratio() const {
    if (period_returns_.size() < cfg_.min_periods_for_risk_metrics) {
        return std::nullopt;
    }
    double mean = 0.0;
    for (double r : period_returns_) {
        mean += r;
    }
    mean /= static_cast<double>(period_returns_.size());

    double var = 0.0;
    for (double r : period_returns_) {
        const double d = r - mean;
        var += d * d;
    }
    if (period_returns_.size() < 2) {
        return std::nullopt;
    }
    var /= static_cast<double>(period_returns_.size() - 1);
    const double stdev = std::sqrt(std::max(0.0, var));
    if (stdev < 1e-18) {
        return std::nullopt;
    }
    return (mean / stdev) * std::sqrt(cfg_.periods_per_year);
}

std::optional<double> RiskManager::sortino_ratio() const {
    if (period_returns_.size() < cfg_.min_periods_for_risk_metrics) {
        return std::nullopt;
    }
    double mean = 0.0;
    for (double r : period_returns_) {
        mean += r;
    }
    mean /= static_cast<double>(period_returns_.size());

    double down = 0.0;
    for (double r : period_returns_) {
        const double v = std::min(0.0, r);
        down += v * v;
    }
    down /= static_cast<double>(period_returns_.size());
    const double dd = std::sqrt(std::max(0.0, down));
    if (dd < 1e-18) {
        return std::nullopt;
    }
    return (mean / dd) * std::sqrt(cfg_.periods_per_year);
}

double RiskManager::fill_rate() const {
    if (orders_submitted_ == 0) {
        return 0.0;
    }
    return static_cast<double>(fills_recorded_) / static_cast<double>(orders_submitted_);
}

double RiskManager::average_spread_captured() const {
    if (spread_capture_samples_ == 0) {
        return 0.0;
    }
    return spread_capture_sum_ / static_cast<double>(spread_capture_samples_);
}

double RiskManager::mean_inventory() const {
    if (inventory_samples_ == 0) {
        return 0.0;
    }
    return inventory_sum_ / static_cast<double>(inventory_samples_);
}

double RiskManager::variance_inventory() const {
    if (inventory_samples_ < 2) {
        return 0.0;
    }
    const double n = static_cast<double>(inventory_samples_);
    const double mean = inventory_sum_ / n;
    const double m2 = inventory_sum_sq_ / n;
    return std::max(0.0, m2 - mean * mean);
}

} // namespace mmsim
