#pragma once

#include "latency_lab/order_book.hpp"

#include <string>

namespace latency_lab {

struct CommandResult {
    std::string output;
    bool quit = false;
};

class CommandProcessor {
public:
    CommandResult process(std::string line);

private:
    OrderBook book_;
};

}
