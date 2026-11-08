cmake_minimum_required(VERSION 3.16)
project(latency_lab LANGUAGES CXX)

add_library(order_book
    src/order_book.cpp
    src/command_processor.cpp
)

target_include_directories(order_book PUBLIC include)
target_compile_features(order_book PUBLIC cxx_std_20)
target_compile_options(order_book PRIVATE -Wall -Wextra -Wpedantic)

add_executable(latency_cli src/main.cpp)
target_link_libraries(latency_cli PRIVATE order_book)

add_executable(latency_server src/tcp_server.cpp)
target_link_libraries(latency_server PRIVATE order_book)
target_compile_options(latency_server PRIVATE -Wall -Wextra -Wpedantic)

add_executable(order_book_tests tests/order_book_tests.cpp)
target_link_libraries(order_book_tests PRIVATE order_book)

find_package(Threads REQUIRED)
target_link_libraries(latency_server PRIVATE Threads::Threads)

find_package(Python3 COMPONENTS Interpreter REQUIRED)

enable_testing()

add_test(
    NAME order_book_tests
    COMMAND order_book_tests
)

add_test(
    NAME differential_tests
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tests/differential_test.py
            $<TARGET_FILE:latency_cli>
)

add_test(
    NAME tcp_server_tests
    COMMAND ${CMAKE_COMMAND} -E env
            "LATENCY_SERVER_BIN=$<TARGET_FILE:latency_server>"
            ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tests/tcp_server_tests.py
)

add_test(
    NAME concurrent_server_tests
    COMMAND ${CMAKE_COMMAND} -E env
            "LATENCY_SERVER_BIN=$<TARGET_FILE:latency_server>"
            ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tests/concurrent_server_tests.py
)

add_executable(latency_bench benchmarks/matching_benchmark.cpp)
target_link_libraries(latency_bench PRIVATE order_book)
target_compile_options(latency_bench PRIVATE -Wall -Wextra -Wpedantic)

add_test(
    NAME matching_benchmark_smoke
    COMMAND ${CMAKE_COMMAND} -E env
            "LATENCY_BENCH_BIN=$<TARGET_FILE:latency_bench>"
            ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tests/benchmark_smoke_test.py
)

add_test(
    NAME tcp_benchmark_smoke
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tests/tcp_benchmark_smoke_test.py
            $<TARGET_FILE:latency_server>
)