#pragma once

#include "mmsim/avellaneda_stoikov.h"
#include "mmsim/types.h"

#include <optional>

namespace mmsim {

/// Naive market maker: symmetric quotes around mid with constant half-spread (raw price units).
[[nodiscard]] inline std::optional<AsQuotes> fixed_spread_quotes(Price mid,
                                                                 Price half_spread) noexcept {
    if (half_spread <= 0) {
        return std::nullopt;
    }
    AsQuotes q;
    q.bid = mid - half_spread;
    q.ask = mid + half_spread;
    if (q.bid >= q.ask) {
        return std::nullopt;
    }
    return q;
}

} // namespace mmsim
