#pragma once

#include "mmsim/types.h"

#include <cstddef>
#include <cstdint>
#include <string>

#include <nlohmann/json.hpp>

namespace mmsim::ws {

/// Parameters for offline replay (same tick stream for each strategy run).
struct ReplayOptions {
    std::string data_path;
    std::size_t bus_capacity = 65536;

    double gamma = 0.5;
    double kappa = 1.0;
    mmsim::Quantity quote_size = 5;
    double sigma = 0.15;
    double tau = 1.0;
    bool tau_decay = true;

    mmsim::Quantity max_position = 500;
    double periods_per_year = 252.0;

    /// Half-width of each side of the quote (fixed-spread MM); full spread = 2 * half.
    mmsim::Price fixed_half_spread = 100;
};

/// Run Avellaneda–Stoikov and fixed-spread replays; returns JSON with `markdown` plus structured
/// fields.
[[nodiscard]] nlohmann::json run_strategy_comparison(const ReplayOptions& opt);

} // namespace mmsim::ws
