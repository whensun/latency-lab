#include "latency_lab/order_book.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;
using Nanoseconds = std::chrono::nanoseconds;

struct OrderIds {
    std::vector<std::string> makers;
    std::vector<std::string> takers;
};

std::size_t parse_samples(int argc, char** argv) {
    if (argc == 1) {
        return 20000;
    }

    if (argc != 2) {
        throw std::invalid_argument("Usage: latency_bench [samples]");
    }

    const std::string argument = argv[1];

    if (argument.empty() || !std::all_of(
        argument.begin(), argument.end(), [](unsigned char c) {
            return std::isdigit(c) != 0;
        }
    )) {
        throw std::invalid_argument(
            "samples must be an integer from 1 to 1000000"
        );
    }

    const auto samples = std::stoull(argument);

    if (samples < 1 || samples > 1000000) {
        throw std::invalid_argument(
            "samples must be an integer from 1 to 1000000"
        );
    }

    return static_cast<std::size_t>(samples);
}

OrderIds make_ids(std::size_t count) {
    OrderIds ids;
    ids.makers.reserve(count);
    ids.takers.reserve(count);

    for (std::size_t i = 0; i < count; ++i) {
        ids.makers.push_back("maker_" + std::to_string(i));
        ids.takers.push_back("taker_" + std::to_string(i));
    }

    return ids;
}

std::uint64_t run_pairs(
    latency_lab::OrderBook& book,
    const OrderIds& ids,
    std::vector<std::int64_t>* latency_ns
) {
    std::uint64_t matched_quantity = 0;

    for (std::size_t i = 0; i < ids.makers.size(); ++i) {
        const auto resting = book.add(
            ids.makers[i], latency_lab::Side::Sell, 100, 1
        );

        if (!resting.empty()) {
            throw std::runtime_error("unexpected resting-order trade");
        }

        const auto start = latency_ns
            ? Clock::now()
            : Clock::time_point{};

        const auto trades = book.add(
            ids.takers[i], latency_lab::Side::Buy, 100, 1
        );

        const auto finish = latency_ns
            ? Clock::now()
            : Clock::time_point{};

        if (
            trades.size() != 1 ||
            trades[0].price != 100 ||
            trades[0].quantity != 1
        ) {
            throw std::runtime_error("unexpected matching result");
        }

        matched_quantity += static_cast<std::uint64_t>(
            trades[0].quantity
        );

        if (latency_ns) {
            latency_ns->push_back(
                std::chrono::duration_cast<Nanoseconds>(
                    finish - start
                ).count()
            );
        }
    }

    if (book.active_orders() != 0) {
        throw std::runtime_error(
            "order book was not cleared after matching"
        );
    }

    return matched_quantity;
}

std::int64_t percentile(
    const std::vector<std::int64_t>& sorted,
    std::size_t percent
) {
    const std::size_t position =
        ((sorted.size() * percent + 99) / 100) - 1;

    return sorted[position];
}

}

int main(int argc, char** argv) {
    try {
        const auto samples = parse_samples(argc, argv);

        const auto warmup_ids = make_ids(5000);
        const auto sample_ids = make_ids(samples);

        latency_lab::OrderBook warmup_book;
        run_pairs(warmup_book, warmup_ids, nullptr);

        latency_lab::OrderBook latency_book;
        std::vector<std::int64_t> latencies;
        latencies.reserve(samples);

        const auto latency_checksum = run_pairs(
            latency_book, sample_ids, &latencies
        );

        std::sort(latencies.begin(), latencies.end());

        long double sum_ns = 0;

        for (const auto latency : latencies) {
            sum_ns += latency;
        }

        latency_lab::OrderBook throughput_book;

        const auto start = Clock::now();

        const auto throughput_checksum = run_pairs(
            throughput_book, sample_ids, nullptr
        );

        const auto finish = Clock::now();

        const double seconds =
            std::chrono::duration<double>(finish - start).count();

        const double throughput =
            (2.0 * static_cast<double>(samples)) / seconds;

        std::cout
            << "samples=" << samples << '\n'
            << "warmup_pairs=" << warmup_ids.makers.size() << '\n'
            << "latency_scope=crossing_buy_matching_one_resting_sell\n"
            << "p50_ns=" << percentile(latencies, 50) << '\n'
            << "p95_ns=" << percentile(latencies, 95) << '\n'
            << "p99_ns=" << percentile(latencies, 99) << '\n'
            << "mean_ns=" << std::fixed << std::setprecision(2)
            << (sum_ns / samples) << '\n'
            << "throughput_scope=single_thread_sell_then_buy_pairs\n"
            << "throughput_orders_per_sec="
            << std::fixed << std::setprecision(2)
            << throughput << '\n'
            << "matched_quantity=" << latency_checksum << '\n'
            << "throughput_matched_quantity="
            << throughput_checksum << '\n';

    } catch (const std::exception& error) {
        std::cerr << "ERROR " << error.what() << '\n';
        return 1;
    }
}
