#include "latency_lab/command_processor.hpp"

#include <iostream>
#include <string>

int main() {
    latency_lab::CommandProcessor processor;

    std::cout
        << "Commands: BUY id price qty | SELL id price qty"
        << " | CANCEL id | BOOK | QUIT\n"
        << "Prices are integer ticks, not floating-point dollars.\n";

    std::string line;

    while (std::cout << "> " && std::getline(std::cin, line)) {
        const auto result = processor.process(line);

        if (result.quit) {
            break;
        }

        std::cout << result.output;
    }
}
