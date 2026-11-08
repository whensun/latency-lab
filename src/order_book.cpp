
#include "latency_lab/order_book.hpp"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace latency_lab {

std::vector<Trade> OrderBook::add(
    std::string id,
    Side side,
    Price price,
    Quantity quantity
) {
    if (id.empty() || price <= 0 || quantity <= 0) {
        throw std::invalid_argument(
            "id must be nonempty; price and quantity must be positive"
        );
    }

    if (active_.contains(id)) {
        throw std::invalid_argument("duplicate active order id");
    }

    Order incoming{std::move(id), side, price, quantity};
    std::vector<Trade> trades;

    if (side == Side::Buy) {
        while (
            incoming.remaining > 0 &&
            !asks_.empty() &&
            asks_.begin()->first <= price
        ) {
            auto level = asks_.begin();
            auto& queue = level->second;
            auto& maker = queue.front();

            const Quantity filled = std::min(
                incoming.remaining,
                maker.remaining
            );

            trades.push_back({
                incoming.id, maker.id, maker.price, filled
            });

            incoming.remaining -= filled;
            maker.remaining -= filled;

            if (maker.remaining == 0) {
                active_.erase(maker.id);
                queue.pop_front();

                if (queue.empty()) {
                    asks_.erase(level);
                }
            }
        }

        if (incoming.remaining > 0) {
            auto& queue = bids_[price];
            queue.push_back(std::move(incoming));

            active_.emplace(
                queue.back().id,
                Location{
                    Side::Buy,
                    price,
                    std::prev(queue.end())
                }
            );
        }
    } else {
        while (
            incoming.remaining > 0 &&
            !bids_.empty() &&
            bids_.begin()->first >= price
        ) {
            auto level = bids_.begin();
            auto& queue = level->second;
            auto& maker = queue.front();

            const Quantity filled = std::min(
                incoming.remaining,
                maker.remaining
            );

            trades.push_back({
                maker.id, incoming.id, maker.price, filled
            });

            incoming.remaining -= filled;
            maker.remaining -= filled;

            if (maker.remaining == 0) {
                active_.erase(maker.id);
                queue.pop_front();

                if (queue.empty()) {
                    bids_.erase(level);
                }
            }
        }

        if (incoming.remaining > 0) {
            auto& queue = asks_[price];
            queue.push_back(std::move(incoming));

            active_.emplace(
                queue.back().id,
                Location{
                    Side::Sell,
                    price,
                    std::prev(queue.end())
                }
            );
        }
    }

    return trades;
}

bool OrderBook::cancel(const std::string& id) {
    auto found = active_.find(id);

    if (found == active_.end()) {
        return false;
    }

    const Location location = found->second;

    if (location.side == Side::Buy) {
        auto level = bids_.find(location.price);

        level->second.erase(location.position);

        if (level->second.empty()) {
            bids_.erase(level);
        }
    } else {
        auto level = asks_.find(location.price);

        level->second.erase(location.position);

        if (level->second.empty()) {
            asks_.erase(level);
        }
    }

    active_.erase(found);
    return true;
}

std::vector<Level> OrderBook::depth(
    Side side,
    std::size_t max_levels
) const {
    if (max_levels == 0) {
        return {};
    }

    const auto collect = [max_levels](const auto& book) {
        std::vector<Level> result;

        for (const auto& [price, queue] : book) {
            Quantity total = 0;

            for (const auto& order : queue) {
                total += order.remaining;
            }

            result.push_back({
                price, total, queue.size()
            });

            if (result.size() == max_levels) {
                break;
            }
        }

        return result;
    };

    return side == Side::Buy
        ? collect(bids_)
        : collect(asks_);
}

std::size_t OrderBook::active_orders() const {
    return active_.size();
}

}
