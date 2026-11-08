#include "latency_lab/command_processor.hpp"

#include <sstream>
#include <stdexcept>
#include <string>

namespace latency_lab {

CommandResult CommandProcessor::process(std::string line) {
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }

    std::istringstream input(line);
    std::ostringstream output;
    std::string command;
    input >> command;

    if (command == "QUIT") {
        return {"BYE\n", true};
    }

    if (command == "BUY" || command == "SELL") {
        std::string id, extra;
        Price price;
        Quantity quantity;

        if (!(input >> id >> price >> quantity) || (input >> extra)) {
            return {"Usage: " + command + " id price qty\n", false};
        }

        try {
            const Side side = command == "BUY" ? Side::Buy : Side::Sell;
            const auto trades = book_.add(id, side, price, quantity);

            output << "ACCEPTED " << id << ", trades=" << trades.size() << '\n';

            for (const auto& trade : trades) {
                output << "TRADE buy=" << trade.buy_id
                       << " sell=" << trade.sell_id
                       << " price=" << trade.price
                       << " qty=" << trade.quantity << '\n';
            }
        } catch (const std::invalid_argument& error) {
            output << "REJECTED: " << error.what() << '\n';
        }
    } else if (command == "CANCEL") {
        std::string id, extra;

        if (!(input >> id) || (input >> extra)) {
            return {"Usage: CANCEL id\n", false};
        }

        output << (book_.cancel(id) ? "CANCELLED " : "NOT FOUND ")
               << id << '\n';
    } else if (command == "BOOK") {
        for (const Side side : {Side::Buy, Side::Sell}) {
            output << (side == Side::Buy ? "BIDS\n" : "ASKS\n");

            for (const auto& level : book_.depth(side)) {
                output << "  price=" << level.price
                       << " qty=" << level.quantity
                       << " orders=" << level.order_count << '\n';
            }
        }
    } else if (!command.empty()) {
        output << "Unknown command\n";
    }

    return {output.str(), false};
}

}
