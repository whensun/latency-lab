# Latency Lab

A C++20 project for studying trading systems, correctness, low-latency networking, and Linux performance analysis.

## Current Features

- Limit order book with price-time priority
- Buy and sell limit orders
- Partial and full fills
- Order cancellation
- Deterministic matching
- Automated C++ correctness test cases
- Python reference model and differential testing
- TCP server with persistent in-memory order book
- TCP integration testing
- Interactive command-line interface

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

## Run

Start the interactive command-line interface:

```sh
./build/latency_cli
```

## Test

Run all automated test suites:

```sh
ctest --test-dir build --output-on-failure
```

The test suites cover:

- C++ order book correctness
- Python differential testing
- TCP server integration

## TCP Server

Start the TCP server:

```sh
./build/latency_server
```

The server listens on `127.0.0.1:9000` by default.

Supported commands:

- `BUY <id> <price> <quantity>`
- `SELL <id> <price> <quantity>`
- `CANCEL <id>`
- `BOOK`
- `QUIT`

Each command must end with a newline.
Each response ends with an `END` line.

The server currently handles one client at a time.
It shares one in-memory order book across sequential client connections.

## Planned Features

- Concurrent order submission
- UDP networking
- Performance benchmarking
- Linux perf and eBPF diagnostics
- Reproducible performance evaluation
- 