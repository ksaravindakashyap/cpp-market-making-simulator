#include "mmsim/avellaneda_stoikov.h"

#include <cmath>
#include <limits>

namespace mmsim {

namespace {

[[nodiscard]] bool valid_params(double gamma, double kappa, double sigma, double tau) noexcept {
    if (!(gamma > 0.0) || !(kappa > 0.0) || !(sigma >= 0.0) || !(tau >= 0.0)) {
        return false;
    }
    if (!std::isfinite(gamma) || !std::isfinite(kappa) || !std::isfinite(sigma) ||
        !std::isfinite(tau)) {
        return false;
    }
    const double ratio = gamma / kappa;
    if (!std::isfinite(ratio) || ratio <= -1.0) {
        return false;
    }
    return true;
}

} // namespace

AvellanedaStoikovStrategy::AvellanedaStoikovStrategy(Config config)
    : gamma_{config.gamma}, kappa_{config.kappa} {}

double AvellanedaStoikovStrategy::reservation_price(Price mid, Quantity inventory, double sigma,
                                                    double time_remaining) const {
    if (!valid_params(gamma_, kappa_, sigma, time_remaining)) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    const double m = static_cast<double>(mid);
    const double q = static_cast<double>(inventory);
    return m - q * gamma_ * sigma * sigma * time_remaining;
}

double AvellanedaStoikovStrategy::optimal_spread(double sigma, double time_remaining) const {
    if (!valid_params(gamma_, kappa_, sigma, time_remaining)) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    const double intensity_term = (2.0 / gamma_) * std::log(1.0 + gamma_ / kappa_);
    return gamma_ * sigma * sigma * time_remaining + intensity_term;
}

std::optional<AsQuotes> AvellanedaStoikovStrategy::optimal_quotes(Price mid, Quantity inventory,
                                                                  double sigma,
                                                                  double time_remaining) const {
    if (!valid_params(gamma_, kappa_, sigma, time_remaining)) {
        return std::nullopt;
    }
    const double r = reservation_price(mid, inventory, sigma, time_remaining);
    const double spread = optimal_spread(sigma, time_remaining);
    if (!std::isfinite(r) || !std::isfinite(spread) || spread < 0.0) {
        return std::nullopt;
    }

    const double bid_d = r - spread * 0.5;
    const double ask_d = r + spread * 0.5;
    if (!std::isfinite(bid_d) || !std::isfinite(ask_d) || bid_d >= ask_d) {
        return std::nullopt;
    }

    AsQuotes out;
    out.bid = static_cast<Price>(std::llround(bid_d));
    out.ask = static_cast<Price>(std::llround(ask_d));
    if (out.bid >= out.ask) {
        return std::nullopt;
    }
    return out;
}

void AvellanedaStoikovStrategy::on_tick(
    Price mid, Quantity inventory, double sigma, double time_remaining,
    const std::function<void(Price bid, Price ask)>& update_quotes) const {
    const auto q = optimal_quotes(mid, inventory, sigma, time_remaining);
    if (!q.has_value()) {
        return;
    }
    update_quotes(q->bid, q->ask);
}

} // namespace mmsim
