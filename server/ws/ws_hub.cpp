#include "ws_hub.hpp"

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <chrono>
#include <iostream>
#include <set>

namespace mmsim::ws {

namespace {

using websocketpp::connection_hdl;

using WsServer = websocketpp::server<websocketpp::config::asio>;

nlohmann::json wrap_ch(std::string_view ch, nlohmann::json data) {
    return {{"channel", std::string(ch)}, {"data", std::move(data)}};
}

} // namespace

struct WsHub::Impl {
    WsServer server;
    std::set<connection_hdl, std::owner_less<connection_hdl>> connections;
    std::mutex conn_mu;

    std::mutex control_mu;
    std::deque<nlohmann::json> control_queue;

    std::function<nlohmann::json()> book_fn;
    std::function<nlohmann::json()> pnl_fn;
    std::function<nlohmann::json()> strategy_fn;
    std::function<nlohmann::json()> analytics_fn;
    std::function<nlohmann::json()> volatility_fn;
    ControlHandler on_control;

    std::thread io_thread;
    std::thread ticker_thread;
    std::atomic<bool> stop_ticker{false};

    void broadcast_text(const std::string& payload) {
        std::lock_guard<std::mutex> lock(conn_mu);
        for (const auto& hdl : connections) {
            try {
                server.send(hdl, payload, websocketpp::frame::opcode::text);
            } catch (const std::exception& e) {
                (void)e;
            }
        }
    }

    void send_channel(std::string_view channel, const nlohmann::json& data) {
        const nlohmann::json msg = wrap_ch(channel, data);
        broadcast_text(msg.dump());
    }

    void push_control(const nlohmann::json& j) {
        {
            std::lock_guard<std::mutex> lock(control_mu);
            control_queue.push_back(j);
        }
        if (on_control) {
            on_control(j);
        }
    }
};

WsHub::WsHub() = default;

WsHub::~WsHub() {
    stop();
}

void WsHub::start(std::uint16_t port, std::function<nlohmann::json()> book_fn,
                  std::function<nlohmann::json()> pnl_fn,
                  std::function<nlohmann::json()> strategy_fn,
                  std::function<nlohmann::json()> analytics_fn,
                  std::function<nlohmann::json()> volatility_fn, ControlHandler on_control) {
    stop();
    impl_ = std::make_unique<Impl>();
    impl_->book_fn = std::move(book_fn);
    impl_->pnl_fn = std::move(pnl_fn);
    impl_->strategy_fn = std::move(strategy_fn);
    impl_->analytics_fn = std::move(analytics_fn);
    impl_->volatility_fn = std::move(volatility_fn);
    impl_->on_control = std::move(on_control);

    impl_->server.init_asio();
    impl_->server.set_reuse_addr(true);
    impl_->server.clear_access_channels(websocketpp::log::alevel::all);
    impl_->server.clear_error_channels(websocketpp::log::elevel::all);

    impl_->server.set_open_handler([this](connection_hdl hdl) {
        std::lock_guard<std::mutex> lock(impl_->conn_mu);
        impl_->connections.insert(hdl);
    });

    impl_->server.set_close_handler([this](connection_hdl hdl) {
        std::lock_guard<std::mutex> lock(impl_->conn_mu);
        impl_->connections.erase(hdl);
    });

    impl_->server.set_message_handler([this](connection_hdl hdl, WsServer::message_ptr msg) {
        (void)hdl;
        try {
            const auto j = nlohmann::json::parse(msg->get_payload());
            const std::string ch = j.value("channel", "");
            if (ch != "control") {
                return;
            }
            impl_->push_control(j);
        } catch (...) {
        }
    });

    impl_->server.listen(port);
    impl_->server.start_accept();

    running_.store(true);
    impl_->stop_ticker.store(false);

    impl_->io_thread = std::thread([this]() {
        try {
            impl_->server.run();
        } catch (const std::exception& e) {
            std::cerr << "WebSocket io: " << e.what() << '\n';
        }
    });

    // Wall-clock: 100ms book/pnl/strategy; analytics every 1s (every 10th tick).
    impl_->ticker_thread = std::thread([this]() {
        using clock = std::chrono::steady_clock;
        auto next = clock::now();
        int tick = 0;
        while (!impl_->stop_ticker.load()) {
            next += std::chrono::milliseconds(100);
            std::this_thread::sleep_until(next);
            if (impl_->stop_ticker.load()) {
                break;
            }
            ++tick;
            try {
                auto& ios = impl_->server.get_io_service();
                ios.post([this, tick]() {
                    if (!impl_ || impl_->stop_ticker.load()) {
                        return;
                    }
                    try {
                        if (impl_->book_fn) {
                            impl_->send_channel("book", impl_->book_fn());
                        }
                        if (impl_->pnl_fn) {
                            impl_->send_channel("pnl", impl_->pnl_fn());
                        }
                        if (impl_->strategy_fn) {
                            impl_->send_channel("strategy", impl_->strategy_fn());
                        }
                        if (tick % 10 == 0 && impl_->analytics_fn) {
                            impl_->send_channel("analytics", impl_->analytics_fn());
                        }
                        if (impl_->volatility_fn) {
                            impl_->send_channel("volatility", impl_->volatility_fn());
                        }
                    } catch (...) {
                    }
                });
            } catch (...) {
            }
        }
    });
}

void WsHub::stop() {
    if (!impl_) {
        running_.store(false);
        return;
    }
    impl_->stop_ticker.store(true);
    if (impl_->ticker_thread.joinable()) {
        impl_->ticker_thread.join();
    }
    try {
        impl_->server.stop_listening();
    } catch (...) {
    }
    try {
        impl_->server.get_io_service().stop();
    } catch (...) {
    }
    if (impl_->io_thread.joinable()) {
        impl_->io_thread.join();
    }
    impl_.reset();
    running_.store(false);
}

void WsHub::post_trade(const nlohmann::json& trade_data) {
    if (!impl_ || !running_.load()) {
        return;
    }
    try {
        impl_->server.get_io_service().post([this, trade_data]() {
            if (!impl_) {
                return;
            }
            impl_->send_channel("trades", trade_data);
        });
    } catch (...) {
    }
}

void WsHub::broadcast_channel(std::string_view channel, const nlohmann::json& data) {
    if (!impl_ || !running_.load()) {
        return;
    }
    try {
        const std::string ch{channel};
        impl_->server.get_io_service().post([this, ch, data]() {
            if (!impl_) {
                return;
            }
            impl_->send_channel(ch, data);
        });
    } catch (...) {
    }
}

bool WsHub::try_pop_control(nlohmann::json& out) {
    if (!impl_) {
        return false;
    }
    std::lock_guard<std::mutex> lock(impl_->control_mu);
    if (impl_->control_queue.empty()) {
        return false;
    }
    out = std::move(impl_->control_queue.front());
    impl_->control_queue.pop_front();
    return true;
}

} // namespace mmsim::ws
