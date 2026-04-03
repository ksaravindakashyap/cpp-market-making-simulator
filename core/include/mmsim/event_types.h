#pragma once

#include "mmsim/types.h"

#include <cstdint>

namespace mmsim {

enum class EventType : std::uint8_t {
    OrderNew,
    OrderCancel,
    OrderAck,
    OrderReject,
    Fill,
    Trade,
    BookDelta,
    TimerTick,
    MarketTick,
    SessionStart,
    SessionEnd,
};

struct OrderNewData {
    OrderId order_id;
    Side side;
    Price price;
    Quantity quantity;
};

struct OrderCancelData {
    OrderId order_id;
};

struct OrderAckData {
    OrderId order_id;
};

struct OrderRejectData {
    OrderId order_id;
    std::uint32_t reason_code;
};

struct FillData {
    OrderId order_id;
    Price price;
    Quantity quantity;
    std::int64_t fee; // signed, quote currency, same scale as Price if applicable
    /// Same on both legs of a trade: resting (maker) and aggressor (taker).
    OrderId maker_id;
    OrderId taker_id;
    std::uint64_t timestamp_ns;
};

struct TradeData {
    std::uint64_t trade_id;
    Price price;
    Quantity quantity;
    Side aggressor_side;
};

struct BookDeltaData {
    Side side;
    Price price;
    Quantity quantity_delta; // positive = add, negative = remove
};

struct TimerTickData {
    std::uint64_t timestamp_ns;
};

/// Single trade / quote tick from historical or simulated feed (plain struct for `Event` union).
struct MarketTickData {
    std::uint64_t timestamp_ns;
    Price price;
    Quantity quantity;
    Side side;
};

struct SessionStartData {
    std::uint64_t session_id;
};

struct SessionEndData {
    std::uint64_t session_id;
};

struct Event {
    EventType type;

    union {
        OrderNewData order_new;
        OrderCancelData order_cancel;
        OrderAckData order_ack;
        OrderRejectData order_reject;
        FillData fill;
        TradeData trade;
        BookDeltaData book_delta;
        TimerTickData timer_tick;
        MarketTickData market_tick;
        SessionStartData session_start;
        SessionEndData session_end;
    };
};

} // namespace mmsim
