// Lightweight assertion-based tests — no external framework so the repo
// builds anywhere with just a C++17 compiler.

#include "lob/order_book.hpp"

#include <cassert>
#include <iostream>
#include <vector>

using namespace lob;

namespace {

int g_checks = 0;
int g_failed = 0;

void check(bool cond, const char* what) {
    ++g_checks;
    if (!cond) {
        ++g_failed;
        std::cerr << "FAIL: " << what << '\n';
    }
}

Order limit(OrderId id, Side s, Price px, Quantity q, Timestamp ts) {
    return Order{id, s, OrderType::Limit, px, q, ts};
}

// A resting limit that does not cross simply rests; no trades.
void test_resting_limit_no_trade() {
    OrderBook b;
    auto t = b.submit(limit(1, Side::Buy, 100, 10, 1));
    check(t.empty(), "resting limit produces no trade");
    check(b.best_bid().value() == 100, "best bid set");
    check(b.quantity_at(Side::Buy, 100) == 10, "quantity tracked");
}

// A crossing order matches at the resting (maker) price.
void test_simple_cross() {
    OrderBook b;
    b.submit(limit(1, Side::Sell, 101, 5, 1));      // ask 101 x5
    auto t = b.submit(limit(2, Side::Buy, 102, 3, 2)); // buy crosses
    check(t.size() == 1, "one trade on cross");
    check(t[0].price == 101, "executes at maker price");
    check(t[0].quantity == 3, "fills min quantity");
    check(b.quantity_at(Side::Sell, 101) == 2, "maker remainder rests");
    check(!b.best_bid().has_value(), "fully-filled taker does not rest");
}

// Time priority: equal-price resting orders fill oldest-first.
void test_time_priority() {
    OrderBook b;
    b.submit(limit(1, Side::Sell, 100, 4, 1));  // older
    b.submit(limit(2, Side::Sell, 100, 4, 2));  // newer
    auto t = b.submit(limit(3, Side::Buy, 100, 5, 3));
    check(t.size() == 2, "sweeps two makers");
    check(t[0].maker_id == 1, "oldest filled first");
    check(t[1].maker_id == 2, "then newer");
    check(t[0].quantity == 4 && t[1].quantity == 1, "split correctly");
}

// Price priority: best price fills before worse prices.
void test_price_priority() {
    OrderBook b;
    b.submit(limit(1, Side::Sell, 102, 5, 1));
    b.submit(limit(2, Side::Sell, 100, 5, 2));  // better ask
    auto t = b.submit(limit(3, Side::Buy, 103, 5, 3));
    check(t[0].price == 100, "best ask matched first");
}

// Market order sweeps levels regardless of price, never rests.
void test_market_order() {
    OrderBook b;
    b.submit(limit(1, Side::Sell, 100, 2, 1));
    b.submit(limit(2, Side::Sell, 101, 2, 2));
    Order m{3, Side::Buy, OrderType::Market, 0, 10, 3};
    auto t = b.submit(m);
    check(t.size() == 2, "market sweeps both levels");
    check(b.empty(), "market remainder dropped, book empty");
}

// IOC fills available quantity and drops the rest (no resting).
void test_ioc() {
    OrderBook b;
    b.submit(limit(1, Side::Sell, 100, 3, 1));
    Order ioc{2, Side::Buy, OrderType::Ioc, 100, 10, 2};
    auto t = b.submit(ioc);
    check(t.size() == 1 && t[0].quantity == 3, "IOC fills what it can");
    check(!b.best_bid().has_value(), "IOC remainder not rested");
}

// Cancel removes a resting order and cleans up the empty level.
void test_cancel() {
    OrderBook b;
    b.submit(limit(1, Side::Buy, 100, 5, 1));
    check(b.cancel(1), "cancel finds order");
    check(!b.best_bid().has_value(), "level removed when empty");
    check(!b.cancel(1), "double cancel is a no-op");
}

} // namespace

int main() {
    test_resting_limit_no_trade();
    test_simple_cross();
    test_time_priority();
    test_price_priority();
    test_market_order();
    test_ioc();
    test_cancel();

    std::cout << g_checks << " checks, " << g_failed << " failed\n";
    return g_failed == 0 ? 0 : 1;
}
