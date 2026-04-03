#pragma once

#include "mmsim/types.h"

#include <functional>
#include <optional>

namespace mmsim {

struct AsQuotes {
    Price bid = 0;
    Price ask = 0;
};

/// Avellaneda–Stoikov-style quoting: reservation price and spread, then half-spread around
/// reservation. All prices use the same fixed-point `Price` scale as the rest of the simulator.
class AvellanedaStoikovStrategy {
  public:
    struct Config {
        double gamma = 0.0; // risk aversion (> 0)
        double kappa = 0.0; // order arrival intensity scale (> 0), inside ln(1 + gamma/kappa)
    };

    explicit AvellanedaStoikovStrategy(Config config);

    [[nodiscard]] double gamma() const noexcept {
        return gamma_;
    }
    [[nodiscard]] double kappa() const noexcept {
        return kappa_;
    }

    /// r = mid - q * gamma * sigma^2 * (T - t)
    [[nodiscard]] double reservation_price(Price mid, Quantity inventory, double sigma,
                                           double time_remaining) const;

    /// gamma * sigma^2 * (T-t) + (2/gamma) * ln(1 + gamma/kappa)
    [[nodiscard]] double optimal_spread(double sigma, double time_remaining) const;

    /// bid = r - spread/2, ask = r + spread/2 (rounded to nearest tick). Invalid config/inputs ->
    /// nullopt.
    [[nodiscard]] std::optional<AsQuotes> optimal_quotes(Price mid, Quantity inventory,
                                                         double sigma, double time_remaining) const;

    /// Recomputes quotes and invokes `update_quotes(bid, ask)` when a valid quote pair exists.
    void on_tick(Price mid, Quantity inventory, double sigma, double time_remaining,
                 const std::function<void(Price bid, Price ask)>& update_quotes) const;

  private:
    double gamma_{};
    double kappa_{};
};

} // namespace mmsim
