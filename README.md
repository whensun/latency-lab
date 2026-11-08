# Latency Lab

A C++20 project for studying trading systems, correctness,
low-latency networking, and Linux performance analysis.

## Current Features

- Limit order book with price-time priority
- Buy and sell limit orders
- Partial and full fills
- Order cancellation
- Deterministic matching
- Automated correctness test cases
- Interactive command-line interface

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
```

## Run

```sh
./build/latency_cli
```

## Test

```sh
ctest --test-dir build --output-on-failure
```

## Planned Features

- Python reference model and differential testing
- TCP/UDP networking
- Concurrent order submission
- Performance benchmarking
- Linux perf and eBPF diagnostics
- Reproducible performance evaluation
