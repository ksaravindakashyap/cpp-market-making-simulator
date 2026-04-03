#pragma once

#include "mmsim/event_bus.h"
#include "mmsim/event_types.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace mmsim {

class MatchingEngine;

/// Replay pacing relative to timestamps in the file: wall-clock delay = delta_ns / factor.
/// `Max` applies no delay between ticks (still monotonic timestamps in events).
enum class ReplaySpeed : std::uint8_t {
    OneX = 0,
    TenX = 1,
    HundredX = 2,
    Max = 3,
    FiveX = 4,
    FiftyX = 5,
};

/// Loads CSV ticks (`timestamp_ns,price,quantity,side`) and publishes `EventType::MarketTick` on an
/// `EventBus`.
class MarketDataFeed {
  public:
    [[nodiscard]] bool load_csv(std::string_view path);

    void set_replay_speed(ReplaySpeed speed) noexcept {
        speed_ = speed;
    }

    [[nodiscard]] ReplaySpeed replay_speed() const noexcept {
        return speed_;
    }

    [[nodiscard]] std::size_t tick_count() const noexcept {
        return ticks_.size();
    }

    [[nodiscard]] std::size_t next_index() const noexcept {
        return next_index_;
    }

    void reset() noexcept;

    /// Sleeps (unless `Max` speed) for the gap before the next tick, then pushes one `MarketTick`
    /// event. Returns false if there are no more ticks, or if the ring buffer is full (retry
    /// later).
    [[nodiscard]] bool try_push_next(EventBus& bus);

  private:
    void sleep_before_index(std::size_t idx) const;

    std::vector<MarketTickData> ticks_;
    std::size_t next_index_{0};
    bool slept_for_next_emit_{false};
    ReplaySpeed speed_{ReplaySpeed::OneX};
};

/// Executes each tick as a market-style order against `MatchingEngine` (aggressive limit: max buy /
/// min sell).
class SimulatedMarketOrderFlow {
  public:
    explicit SimulatedMarketOrderFlow(MatchingEngine& engine);

    /// Submits one aggressive order derived from the tick; returns aggregate fills from the engine.
    [[nodiscard]] bool execute_tick(const MarketTickData& tick, OrderId order_id,
                                    std::vector<FillData>& fills_out);

  private:
    MatchingEngine& engine_;
};

} // namespace mmsim
