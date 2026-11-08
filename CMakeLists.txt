
cmake_minimum_required(VERSION 3.16)
project(latency_lab LANGUAGES CXX)

add_library(order_book src/order_book.cpp)
target_include_directories(order_book PUBLIC include)
target_compile_features(order_book PUBLIC cxx_std_20)
target_compile_options(order_book PRIVATE -Wall -Wextra -Wpedantic)

add_executable(latency_cli src/main.cpp)
target_link_libraries(latency_cli PRIVATE order_book)

add_executable(order_book_tests tests/order_book_tests.cpp)
target_link_libraries(order_book_tests PRIVATE order_book)

enable_testing()
add_test(NAME order_book_tests COMMAND order_book_tests)
