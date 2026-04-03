#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include <atomic>

#include <nlohmann/json.hpp>

namespace mmsim::ws {

/// WebSocket hub: separate thread runs the Asio / websocketpp loop; timers for 100ms / 1s channels;
/// thread-safe fill posts and control queue for the simulation thread.
class WsHub {
  public:
    using ControlHandler = std::function<void(const nlohmann::json&)>;

    WsHub();
    ~WsHub();

    WsHub(const WsHub&) = delete;
    WsHub& operator=(const WsHub&) = delete;

    /// Starts listening on `port`; schedules periodic broadcasts using the supplied getters
    /// (main-thread state).
    void start(std::uint16_t port, std::function<nlohmann::json()> book_fn,
               std::function<nlohmann::json()> pnl_fn, std::function<nlohmann::json()> strategy_fn,
               std::function<nlohmann::json()> analytics_fn,
               std::function<nlohmann::json()> volatility_fn, ControlHandler on_control);

    void stop();

    [[nodiscard]] bool running() const noexcept {
        return running_.load();
    }

    /// Thread-safe: enqueue a trades-channel message (typically one fill).
    void post_trade(const nlohmann::json& trade_data);

    /// Clear buffered trade rows (call when simulation resets).
    void clear_trade_history();

    /// Thread-safe: broadcast a single JSON payload on any channel (e.g. comparison report).
    void broadcast_channel(std::string_view channel, const nlohmann::json& data);

    /// Simulation thread: drain control messages (from JSON WebSocket clients).
    [[nodiscard]] bool try_pop_control(nlohmann::json& out);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::atomic<bool> running_{false};
};

} // namespace mmsim::ws
