#pragma once

#include "mmsim/types.h"

#include <cstddef>
#include <optional>
#include <span>

namespace mmsim {

struct OhlcBar {
    Price open = 0;
    Price high = 0;
    Price low_price = 0;
    Price close = 0;
};

/// Rolling-window volatility estimators (per-bar units, not annualized).
class VolatilityEstimator {
  public:
    explicit VolatilityEstimator(std::size_t window);

    [[nodiscard]] std::size_t window() const noexcept {
        return window_;
    }
    void set_window(std::size_t window) noexcept {
        window_ = window;
    }

    /// Sample standard deviation of log closes over the last `window` returns.
    /// Requires `closes.size() >= window + 1` (strictly positive prices).
    [[nodiscard]] std::optional<double> close_to_close(std::span<const Price> closes) const;

    /// Parkinson (1980) range estimator using high–low range per bar.
    /// Requires `bars.size() >= window`, high >= low_price > 0.
    [[nodiscard]] std::optional<double> parkinson(std::span<const OhlcBar> bars) const;

    /// Yang–Zhang (2000) OHLC estimator; uses the last `window` bars and the preceding close.
    /// Requires `history.size() >= window + 1` (oldest first), `window >= 2`.
    [[nodiscard]] std::optional<double> yang_zhang(std::span<const OhlcBar> history) const;

  private:
    std::size_t window_;
};

} // namespace mmsim
