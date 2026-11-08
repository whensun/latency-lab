
#include "latency_lab/order_book.hpp"

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

using latency_lab::OrderBook;
using latency_lab::Side;

int main() {
    OrderBook book;

    std::cout
        << "Commands: BUY id price qty | SELL id price qty"
        << " | CANCEL id | BOOK | QUIT\n";

    std::cout
        << "Prices are integer ticks, not floating-point dollars.\n";

    std::string line;

    while (std::cout << "> " && std::getline(std::cin, line)) {
        std::istringstream input(line);
        std::string command;
        input >> command;

        if (command == "QUIT") {
            break;
        }

        if (command == "BUY" || command == "SELL") {
            std::string id, extra;
            latency_lab::Price price;
            latency_lab::Quantity quantity;

            if (!(input >> id >> price >> quantity) || (input >> extra)) {
                std::cout << "Usage: " << command << " id price qty\n";
                continue;
            }

            try {
                const Side side = command == "BUY"
                    ? Side::Buy
                    : Side::Sell;

                const auto trades = book.add(
                    id, side, price, quantity
                );

                std::cout << "ACCEPTED " << id
                          << ", trades=" << trades.size() << '\n';

                for (const auto& trade : trades) {
                    std::cout
                        << "TRADE buy=" << trade.buy_id
                        << " sell=" << trade.sell_id
                        << " price=" << trade.price
                        << " qty=" << trade.quantity
                        << '\n';
                }
            } catch (const std::invalid_argument& error) {
                std::cout << "REJECTED: "
                          << error.what() << '\n';
            }
        } else if (command == "CANCEL") {
            std::string id, extra;

            if (!(input >> id) || (input >> extra)) {
                std::cout << "Usage: CANCEL id\n";
                continue;
            }

            std::cout
                << (book.cancel(id) ? "CANCELLED " : "NOT FOUND ")
                << id << '\n';

        } else if (command == "BOOK") {
            for (const Side side : {Side::Buy, Side::Sell}) {
                std::cout
                    << (side == Side::Buy ? "BIDS\n" : "ASKS\n");

                for (const auto& level : book.depth(side)) {
                    std::cout
                        << "  price=" << level.price
                        << " qty=" << level.quantity
                        << " orders=" << level.order_count
                        << '\n';
                }
            }
        } else if (!command.empty()) {
            std::cout << "Unknown command\n";
        }
    }
}
