#pragma once

#include <cstdint>
#include <string>

namespace lob {

using OrderId   = std::uint64_t;
using Price     = std::int64_t;   // price in integer ticks (avoids floating-point error)
using Quantity  = std::uint64_t;
using Timestamp = std::uint64_t;  // monotonic sequence / nanoseconds

enum class Side : std::uint8_t { Buy, Sell };

enum class OrderType : std::uint8_t {
    Limit,   // rest on the book if not fully filled
    Market,  // match against best available, never rests
    Ioc      // immediate-or-cancel: fill what you can, drop the rest
};

struct Order {
    OrderId   id        = 0;
    Side      side      = Side::Buy;
    OrderType type      = OrderType::Limit;
    Price     price     = 0;   // ignored for Market orders
    Quantity  quantity  = 0;   // remaining quantity
    Timestamp ts        = 0;
};

// A fill produced when an aggressive order crosses a resting order.
struct Trade {
    OrderId   taker_id = 0;   // incoming, aggressive order
    OrderId   maker_id = 0;   // resting, passive order
    Price     price    = 0;   // execution price = resting (maker) price
    Quantity  quantity = 0;
    Timestamp ts       = 0;
};

inline const char* to_string(Side s) { return s == Side::Buy ? "BUY" : "SELL"; }

} // namespace lob
