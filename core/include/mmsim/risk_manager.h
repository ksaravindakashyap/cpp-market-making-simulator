#pragma once

#include "mmsim/event_types.h"
#include "mmsim/types.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace mmsim {

/// Portfolio risk and analytics: PnL, drawdown, Sharpe/Sortino, fill quality, inventory stats.
/// PnL is accumulated in **raw** units: `price * quantity` uses integer `Price` and `Quantity`
/// as-is (same scale as the rest of the simulator); fees subtract from realized PnL.
class RiskManager {
  public:
    struct Config {
        /// Maximum absolute net position; `0` means **no limit**.
        Quantity max_position = 0;
        /// Minimum number of period returns required before Sharpe/Sortino are defined.
        std::size_t min_periods_for_risk_metrics = 2;
        /// Annualization factor, e.g. `252` for daily equity points, or `252 * 390` for minute
        /// bars.
        double periods_per_year = 252.0;
    };

    explicit RiskManager(Config config);

    [[nodiscard]] const Config& config() const noexcept {
        return cfg_;
    }

    void set_max_position(Quantity max_position) noexcept {
        cfg_.max_position = max_position;
    }

    void set_periods_per_year(double periods_per_year) noexcept {
        cfg_.periods_per_year = periods_per_year;
    }

    /// Reset portfolio and analytics to initial state; keeps `config()` limits.
    void reset() noexcept;

    /// Returns false if executing the order would leave `abs(net_position) > max_position` (when
    /// enabled).
    [[nodiscard]] bool allow_order(Side side, Quantity quantity) const;

    /// Register intent to trade `quantity` on `side` (for fill-rate denominator). Optional.
    void on_order_submitted();

    /// Process a fill: updates inventory, realized PnL (average-cost method), spread capture, fill
    /// counts. `mid_at_fill` is the reference mid for spread-captured statistics (optional).
    void on_fill(const FillData& fill, Side side, std::optional<Price> mid_at_fill = std::nullopt);

    /// Mark-to-market using `mark_price`; refreshes unrealized PnL, equity path, drawdown, period
    /// returns.
    void mark(Price mark_price);

    [[nodiscard]] Quantity position() const noexcept {
        return position_;
    }

    [[nodiscard]] double realized_pnl() const noexcept {
        return realized_pnl_;
    }

    [[nodiscard]] double unrealized_pnl() const noexcept {
        return unrealized_pnl_;
    }

    /// `realized_pnl + unrealized_pnl` after the last `mark()`.
    [[nodiscard]] double equity() const noexcept {
        return realized_pnl_ + unrealized_pnl_;
    }

    [[nodiscard]] double max_drawdown() const noexcept {
        return max_drawdown_;
    }

    /// Annualized Sharpe on stored period returns (simple returns from equity changes). Optional if
    /// too few samples.
    [[nodiscard]] std::optional<double> sharpe_ratio() const;

    /// Annualized Sortino (downside deviation of returns below 0). Optional if too few samples.
    [[nodiscard]] std::optional<double> sortino_ratio() const;

    /// `fills_recorded / orders_submitted` if submissions tracked; otherwise uses fill count as
    /// denominator fallback.
    [[nodiscard]] double fill_rate() const;

    /// Average half-spread captured vs `mid_at_fill` when provided on fills; otherwise 0.
    [[nodiscard]] double average_spread_captured() const;

    [[nodiscard]] double mean_inventory() const;
    [[nodiscard]] double variance_inventory() const;
    [[nodiscard]] Quantity max_abs_inventory_observed() const noexcept {
        return max_abs_inventory_;
    }

    [[nodiscard]] std::uint64_t orders_submitted() const noexcept {
        return orders_submitted_;
    }
    [[nodiscard]] std::uint64_t fills_recorded() const noexcept {
        return fills_recorded_;
    }

  private:
    void recompute_unrealized(Price mark_price);
    void update_equity_stats(double equity);

    Config cfg_;
    Quantity position_{0};
    double avg_price_{0.0}; // meaningful when position_ != 0
    double realized_pnl_{0.0};
    double unrealized_pnl_{0.0};

    double peak_equity_{0.0};
    double max_drawdown_{0.0};
    double last_equity_for_return_{0.0};
    bool have_last_equity_{false};

    std::vector<double> period_returns_;

    std::uint64_t orders_submitted_{0};
    std::uint64_t fills_recorded_{0};

    double spread_capture_sum_{0.0};
    std::uint64_t spread_capture_samples_{0};

    double inventory_sum_{0.0};
    double inventory_sum_sq_{0.0};
    std::uint64_t inventory_samples_{0};
    Quantity max_abs_inventory_{0};
};

} // namespace mmsim
