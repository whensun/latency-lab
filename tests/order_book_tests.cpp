#include "latency_lab/order_book.hpp"

#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using latency_lab::OrderBook;
using latency_lab::Side;

void check(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

int main() {
    struct TestCase {
        std::string name;
        std::function<void()> run;
    };

    const std::vector<TestCase> cases = {
        {"resting bid", [] {
            OrderBook book;
            check(
                book.add("b1", Side::Buy, 100, 5).empty(),
                "unexpected trade"
            );
            check(
                book.depth(Side::Buy).at(0).quantity == 5,
                "wrong bid quantity"
            );
            check(book.active_orders() == 1, "wrong active count");
        }},
        {"buy crosses ask at maker price", [] {
            OrderBook book;
            book.add("s1", Side::Sell, 99, 3);
            auto trades = book.add("b1", Side::Buy, 101, 3);

            check(trades.size() == 1, "expected one trade");
            check(
                trades[0].price == 99 && trades[0].quantity == 3,
                "wrong execution"
            );
            check(
                trades[0].buy_id == "b1" &&
                trades[0].sell_id == "s1",
                "wrong ids"
            );
            check(book.active_orders() == 0, "orders should be filled");
        }},
        {"sell crosses bid at maker price", [] {
            OrderBook book;
            book.add("b1", Side::Buy, 105, 3);
            auto trades = book.add("s1", Side::Sell, 100, 3);

            check(
                trades.size() == 1 && trades[0].price == 105,
                "wrong execution price"
            );
        }},
        {"partial maker fill", [] {
            OrderBook book;
            book.add("s1", Side::Sell, 100, 8);
            book.add("b1", Side::Buy, 100, 3);

            check(
                book.depth(Side::Sell).at(0).quantity == 5,
                "wrong remaining quantity"
            );
        }},
        {"incoming remainder rests", [] {
            OrderBook book;
            book.add("s1", Side::Sell, 100, 2);
            auto trades = book.add("b1", Side::Buy, 101, 5);

            check(
                trades.size() == 1 && trades[0].quantity == 2,
                "wrong fill"
            );
            check(
                book.depth(Side::Buy).at(0).quantity == 3,
                "wrong remainder"
            );
        }},
        {"best price first", [] {
            OrderBook book;
            book.add("s100", Side::Sell, 100, 1);
            book.add("s99", Side::Sell, 99, 1);
            auto trades = book.add("b1", Side::Buy, 101, 2);

            check(trades.size() == 2, "expected two trades");
            check(
                trades[0].price == 99 && trades[1].price == 100,
                "price priority broken"
            );
        }},
        {"FIFO at same price", [] {
            OrderBook book;
            book.add("s1", Side::Sell, 100, 1);
            book.add("s2", Side::Sell, 100, 1);
            auto trades = book.add("b1", Side::Buy, 100, 2);

            check(trades.size() == 2, "expected two trades");
            check(
                trades[0].sell_id == "s1" &&
                trades[1].sell_id == "s2",
                "FIFO broken"
            );
        }},
        {"cancel removes resting order", [] {
            OrderBook book;
            book.add("b1", Side::Buy, 100, 3);

            check(book.cancel("b1"), "cancel failed");
            check(!book.cancel("b1"), "duplicate cancel succeeded");
            check(
                book.depth(Side::Buy).empty(),
                "level should be removed"
            );
            check(book.active_orders() == 0, "index not cleared");
        }},
        {"cancel preserves FIFO", [] {
            OrderBook book;
            book.add("b1", Side::Buy, 100, 1);
            book.add("b2", Side::Buy, 100, 1);
            book.add("b3", Side::Buy, 100, 1);
            book.cancel("b2");

            auto trades = book.add("s1", Side::Sell, 100, 2);

            check(trades.size() == 2, "expected two trades");
            check(
                trades[0].buy_id == "b1" &&
                trades[1].buy_id == "b3",
                "FIFO broken"
            );
        }},
        {"noncrossing orders rest", [] {
            OrderBook book;
            book.add("b1", Side::Buy, 99, 1);

            check(
                book.add("s1", Side::Sell, 100, 1).empty(),
                "should not trade"
            );
            check(book.active_orders() == 2, "both should rest");
        }},
        {"invalid and duplicate input", [] {
            OrderBook book;
            book.add("b1", Side::Buy, 100, 1);

            for (int choice = 0; choice < 4; ++choice) {
                bool rejected = false;

                try {
                    if (choice == 0)
                        book.add("", Side::Buy, 100, 1);
                    if (choice == 1)
                        book.add("x", Side::Buy, 0, 1);
                    if (choice == 2)
                        book.add("x", Side::Buy, 100, 0);
                    if (choice == 3)
                        book.add("b1", Side::Buy, 100, 1);
                } catch (const std::invalid_argument&) {
                    rejected = true;
                }

                check(rejected, "invalid order was accepted");
            }
        }},
        {"reuse id after cancellation", [] {
            OrderBook book;
            book.add("b1", Side::Buy, 100, 1);
            book.cancel("b1");
            book.add("b1", Side::Buy, 101, 2);

            check(
                book.depth(Side::Buy).at(0).quantity == 2,
                "id reuse failed"
            );
        }}
    };

    std::size_t passed = 0;

    for (const auto& test_case : cases) {
        try {
            test_case.run();
            ++passed;
            std::cout << "PASS " << test_case.name << '\n';
        } catch (const std::exception& error) {
            std::cerr
                << "FAIL " << test_case.name
                << ": " << error.what() << '\n';
            return 1;
        }
    }

    std::cout << passed << " test cases passed\n";
}
