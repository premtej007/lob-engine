// Replays a CSV order stream through the matching engine and prints the
// resulting trades plus the final top-of-book.
//
// CSV columns:  ts,action,id,side,type,price,qty
//   action : NEW | CANCEL
//   side   : BUY | SELL
//   type   : LIMIT | MARKET | IOC
// CANCEL rows only need ts,action,id (other fields may be left as 0).
//
// Usage:  lob_replay data/sample_orders.csv

#include "lob/order_book.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using namespace lob;

namespace {

Side parse_side(const std::string& s) { return s == "BUY" ? Side::Buy : Side::Sell; }

OrderType parse_type(const std::string& s) {
    if (s == "MARKET") return OrderType::Market;
    if (s == "IOC")    return OrderType::Ioc;
    return OrderType::Limit;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <orders.csv>\n";
        return 1;
    }

    std::ifstream in(argv[1]);
    if (!in) {
        std::cerr << "error: cannot open " << argv[1] << "\n";
        return 1;
    }

    std::uint64_t trade_count = 0;
    Quantity      volume      = 0;

    OrderBook book([&](const Trade& t) {
        ++trade_count;
        volume += t.quantity;
        std::cout << "TRADE  taker=" << t.taker_id << " maker=" << t.maker_id
                  << " px=" << t.price << " qty=" << t.quantity << '\n';
    });

    std::string line;
    std::getline(in, line);  // skip header

    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string ts, action, id, side, type, price, qty;
        std::getline(ss, ts, ',');
        std::getline(ss, action, ',');
        std::getline(ss, id, ',');
        std::getline(ss, side, ',');
        std::getline(ss, type, ',');
        std::getline(ss, price, ',');
        std::getline(ss, qty, ',');

        if (action == "CANCEL") {
            book.cancel(std::stoull(id));
            continue;
        }

        Order o;
        o.ts       = std::stoull(ts);
        o.id       = std::stoull(id);
        o.side     = parse_side(side);
        o.type     = parse_type(type);
        o.price    = price.empty() ? 0 : std::stoll(price);
        o.quantity = std::stoull(qty);
        book.submit(o);
    }

    auto bid = book.best_bid();
    auto ask = book.best_ask();
    std::cout << "\n--- summary ---\n";
    std::cout << "trades executed : " << trade_count << '\n';
    std::cout << "volume traded   : " << volume << '\n';
    std::cout << "resting orders  : " << book.resting_order_count() << '\n';
    std::cout << "best bid        : " << (bid ? std::to_string(*bid) : "-") << '\n';
    std::cout << "best ask        : " << (ask ? std::to_string(*ask) : "-") << '\n';
    return 0;
}
