
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <list>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace latency_lab {

enum class Side { Buy, Sell };

using Price = std::int64_t;
using Quantity = std::int64_t;

struct Order {
    std::string id;
    Side side;
    Price price;
    Quantity remaining;
};

struct Trade {
    std::string buy_id;
    std::string sell_id;
    Price price;
    Quantity quantity;
};

struct Level {
    Price price;
    Quantity quantity;
    std::size_t order_count;
};

class OrderBook {
public:
    std::vector<Trade> add(
        std::string id,
        Side side,
        Price price,
        Quantity quantity
    );

    bool cancel(const std::string& id);

    std::vector<Level> depth(
        Side side,
        std::size_t max_levels = 5
    ) const;

    std::size_t active_orders() const;

private:
    using OrderQueue = std::list<Order>;
    using BidBook = std::map<Price, OrderQueue, std::greater<Price>>;
    using AskBook = std::map<Price, OrderQueue>;

    struct Location {
        Side side;
        Price price;
        OrderQueue::iterator position;
    };

    BidBook bids_;
    AskBook asks_;
    std::unordered_map<std::string, Location> active_;
};

}
