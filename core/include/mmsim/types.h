#pragma once

#include <cstdint>

namespace mmsim {

/// Fixed-point price: raw value = price * 10_000 (four decimal places).
using Price = std::int64_t;

using Quantity = std::int64_t;

using OrderId = std::uint64_t;

/// Source / strategy tag for self-trade prevention. Zero = not tagged (matches any counterparty).
using StrategyId = std::uint32_t;

enum class Side : std::uint8_t {
    BUY = 0,
    SELL = 1,
};

} // namespace mmsim
