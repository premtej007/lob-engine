#pragma once

#include "lob/order.hpp"

#include <functional>
#include <list>
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

namespace lob {

// A limit order book with strict price-time (FIFO) priority.
//
// Design notes
// ------------
// * Each side is a std::map keyed by price. Bids are sorted descending and
//   asks ascending, so begin() is always the best price on that side.
// * Every price level holds a std::list<Order> in arrival order, giving
//   time priority within a level and O(1) splice/erase.
// * An id -> iterator index (locator) makes cancel/modify O(1) average.
//
// Complexity: add/cancel are O(log L) in the number of distinct price
// levels L (the map lookup); matching is O(fills). For a portfolio/demo
// engine this is the right trade-off between clarity and speed; the README
// lists how to push it further (intrusive lists, flat arrays, a price ladder).
class OrderBook {
public:
    using TradeCallback = std::function<void(const Trade&)>;

    explicit OrderBook(TradeCallback on_trade = nullptr)
        : on_trade_(std::move(on_trade)) {}

    // Submit an order. Returns the trades it generated (also delivered via the
    // callback if one was supplied). A resting remainder is added to the book
    // for Limit orders only.
    std::vector<Trade> submit(Order order);

    // Cancel a resting order by id. Returns true if it was found and removed.
    bool cancel(OrderId id);

    // --- Top of book / queries -------------------------------------------
    std::optional<Price> best_bid() const;
    std::optional<Price> best_ask() const;
    Quantity quantity_at(Side side, Price price) const;
    std::size_t resting_order_count() const { return locator_.size(); }
    bool empty() const { return bids_.empty() && asks_.empty(); }

private:
    struct Level {
        std::list<Order> orders;   // FIFO: front = oldest = highest priority
        Quantity total = 0;        // cached sum of resting quantity at this level
    };

    // Bids: highest price first. Asks: lowest price first.
    using BidBook = std::map<Price, Level, std::greater<Price>>;
    using AskBook = std::map<Price, Level, std::less<Price>>;

    struct Locator {
        Side side;
        Price price;
        std::list<Order>::iterator it;
    };

    template <typename BookSide>
    std::vector<Trade> match(Order& incoming, BookSide& opposite);

    void rest(const Order& order);
    void emit(const Trade& t, std::vector<Trade>& out);

    BidBook bids_;
    AskBook asks_;
    std::unordered_map<OrderId, Locator> locator_;
    TradeCallback on_trade_;
};

// ---------------------------------------------------------------------------
// Implementation (header-only for easy dropping into other projects)
// ---------------------------------------------------------------------------

inline void OrderBook::emit(const Trade& t, std::vector<Trade>& out) {
    out.push_back(t);
    if (on_trade_) on_trade_(t);
}

// True when `price` is acceptable for the incoming aggressive order on `side`.
inline bool crosses(Side incoming_side, Price incoming_price, Price book_price) {
    return incoming_side == Side::Buy ? incoming_price >= book_price
                                      : incoming_price <= book_price;
}

template <typename BookSide>
std::vector<Trade> OrderBook::match(Order& incoming, BookSide& opposite) {
    std::vector<Trade> trades;
    const bool is_market = incoming.type == OrderType::Market;

    while (incoming.quantity > 0 && !opposite.empty()) {
        auto best_it = opposite.begin();
        const Price level_price = best_it->first;

        // Limit/IOC stop when the book no longer crosses their price.
        if (!is_market && !crosses(incoming.side, incoming.price, level_price))
            break;

        Level& level = best_it->second;
        auto& queue  = level.orders;

        while (incoming.quantity > 0 && !queue.empty()) {
            Order& resting = queue.front();
            const Quantity fill = std::min(incoming.quantity, resting.quantity);

            emit(Trade{incoming.id, resting.id, level_price, fill, incoming.ts}, trades);

            incoming.quantity -= fill;
            resting.quantity  -= fill;
            level.total        -= fill;

            if (resting.quantity == 0) {
                locator_.erase(resting.id);
                queue.pop_front();
            }
        }

        if (queue.empty()) opposite.erase(best_it);
    }
    return trades;
}

inline std::vector<Trade> OrderBook::submit(Order order) {
    std::vector<Trade> trades = (order.side == Side::Buy)
        ? match(order, asks_)
        : match(order, bids_);

    // Only genuine limit orders rest; market and IOC remainders are dropped.
    if (order.quantity > 0 && order.type == OrderType::Limit)
        rest(order);

    return trades;
}

inline void OrderBook::rest(const Order& order) {
    if (order.side == Side::Buy) {
        Level& lvl = bids_[order.price];
        lvl.orders.push_back(order);
        lvl.total += order.quantity;
        locator_[order.id] = Locator{Side::Buy, order.price, std::prev(lvl.orders.end())};
    } else {
        Level& lvl = asks_[order.price];
        lvl.orders.push_back(order);
        lvl.total += order.quantity;
        locator_[order.id] = Locator{Side::Sell, order.price, std::prev(lvl.orders.end())};
    }
}

inline bool OrderBook::cancel(OrderId id) {
    auto loc_it = locator_.find(id);
    if (loc_it == locator_.end()) return false;

    const Locator loc = loc_it->second;
    const Quantity remaining = loc.it->quantity;

    if (loc.side == Side::Buy) {
        auto lvl_it = bids_.find(loc.price);
        lvl_it->second.total -= remaining;
        lvl_it->second.orders.erase(loc.it);
        if (lvl_it->second.orders.empty()) bids_.erase(lvl_it);
    } else {
        auto lvl_it = asks_.find(loc.price);
        lvl_it->second.total -= remaining;
        lvl_it->second.orders.erase(loc.it);
        if (lvl_it->second.orders.empty()) asks_.erase(lvl_it);
    }

    locator_.erase(loc_it);
    return true;
}

inline std::optional<Price> OrderBook::best_bid() const {
    if (bids_.empty()) return std::nullopt;
    return bids_.begin()->first;
}

inline std::optional<Price> OrderBook::best_ask() const {
    if (asks_.empty()) return std::nullopt;
    return asks_.begin()->first;
}

inline Quantity OrderBook::quantity_at(Side side, Price price) const {
    if (side == Side::Buy) {
        auto it = bids_.find(price);
        return it == bids_.end() ? 0 : it->second.total;
    }
    auto it = asks_.find(price);
    return it == asks_.end() ? 0 : it->second.total;
}

} // namespace lob
