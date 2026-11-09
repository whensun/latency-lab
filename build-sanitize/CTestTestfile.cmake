# CMake generated Testfile for 
# Source directory: /home/wensan/latency-lab
# Build directory: /home/wensan/latency-lab/build-sanitize
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(order_book_tests "/home/wensan/latency-lab/build-sanitize/order_book_tests")
set_tests_properties(order_book_tests PROPERTIES  _BACKTRACE_TRIPLES "/home/wensan/latency-lab/CMakeLists.txt;30;add_test;/home/wensan/latency-lab/CMakeLists.txt;0;")
add_test(differential_tests "/usr/bin/python3" "/home/wensan/latency-lab/tests/differential_test.py" "/home/wensan/latency-lab/build-sanitize/latency_cli")
set_tests_properties(differential_tests PROPERTIES  _BACKTRACE_TRIPLES "/home/wensan/latency-lab/CMakeLists.txt;35;add_test;/home/wensan/latency-lab/CMakeLists.txt;0;")
add_test(tcp_server_tests "/usr/bin/cmake" "-E" "env" "LATENCY_SERVER_BIN=/home/wensan/latency-lab/build-sanitize/latency_server" "/usr/bin/python3" "/home/wensan/latency-lab/tests/tcp_server_tests.py")
set_tests_properties(tcp_server_tests PROPERTIES  _BACKTRACE_TRIPLES "/home/wensan/latency-lab/CMakeLists.txt;42;add_test;/home/wensan/latency-lab/CMakeLists.txt;0;")
add_test(concurrent_server_tests "/usr/bin/cmake" "-E" "env" "LATENCY_SERVER_BIN=/home/wensan/latency-lab/build-sanitize/latency_server" "/usr/bin/python3" "/home/wensan/latency-lab/tests/concurrent_server_tests.py")
set_tests_properties(concurrent_server_tests PROPERTIES  _BACKTRACE_TRIPLES "/home/wensan/latency-lab/CMakeLists.txt;50;add_test;/home/wensan/latency-lab/CMakeLists.txt;0;")
add_test(matching_benchmark_smoke "/usr/bin/cmake" "-E" "env" "LATENCY_BENCH_BIN=/home/wensan/latency-lab/build-sanitize/latency_bench" "/usr/bin/python3" "/home/wensan/latency-lab/tests/benchmark_smoke_test.py")
set_tests_properties(matching_benchmark_smoke PROPERTIES  _BACKTRACE_TRIPLES "/home/wensan/latency-lab/CMakeLists.txt;62;add_test;/home/wensan/latency-lab/CMakeLists.txt;0;")
add_test(tcp_benchmark_smoke "/usr/bin/python3" "/home/wensan/latency-lab/tests/tcp_benchmark_smoke_test.py" "/home/wensan/latency-lab/build-sanitize/latency_server")
set_tests_properties(tcp_benchmark_smoke PROPERTIES  _BACKTRACE_TRIPLES "/home/wensan/latency-lab/CMakeLists.txt;70;add_test;/home/wensan/latency-lab/CMakeLists.txt;0;")
add_test(tcp_server_nodelay_tests "/usr/bin/cmake" "-E" "env" "LATENCY_TCP_NODELAY=1" "LATENCY_SERVER_BIN=/home/wensan/latency-lab/build-sanitize/latency_server" "/usr/bin/python3" "/home/wensan/latency-lab/tests/tcp_server_tests.py")
set_tests_properties(tcp_server_nodelay_tests PROPERTIES  _BACKTRACE_TRIPLES "/home/wensan/latency-lab/CMakeLists.txt;77;add_test;/home/wensan/latency-lab/CMakeLists.txt;0;")
add_test(concurrent_nodelay_tests "/usr/bin/cmake" "-E" "env" "LATENCY_TCP_NODELAY=1" "LATENCY_SERVER_BIN=/home/wensan/latency-lab/build-sanitize/latency_server" "/usr/bin/python3" "/home/wensan/latency-lab/tests/concurrent_server_tests.py")
set_tests_properties(concurrent_nodelay_tests PROPERTIES  _BACKTRACE_TRIPLES "/home/wensan/latency-lab/CMakeLists.txt;86;add_test;/home/wensan/latency-lab/CMakeLists.txt;0;")
add_test(tcp_server_batch_tests "/usr/bin/cmake" "-E" "env" "LATENCY_WORKER_BATCH=1" "LATENCY_TCP_NODELAY=1" "LATENCY_SERVER_BIN=/home/wensan/latency-lab/build-sanitize/latency_server" "/usr/bin/python3" "/home/wensan/latency-lab/tests/tcp_server_tests.py")
set_tests_properties(tcp_server_batch_tests PROPERTIES  _BACKTRACE_TRIPLES "/home/wensan/latency-lab/CMakeLists.txt;95;add_test;/home/wensan/latency-lab/CMakeLists.txt;0;")
add_test(concurrent_batch_tests "/usr/bin/cmake" "-E" "env" "LATENCY_WORKER_BATCH=1" "LATENCY_TCP_NODELAY=1" "LATENCY_SERVER_BIN=/home/wensan/latency-lab/build-sanitize/latency_server" "/usr/bin/python3" "/home/wensan/latency-lab/tests/concurrent_server_tests.py")
set_tests_properties(concurrent_batch_tests PROPERTIES  _BACKTRACE_TRIPLES "/home/wensan/latency-lab/CMakeLists.txt;105;add_test;/home/wensan/latency-lab/CMakeLists.txt;0;")
add_test(batch_pipeline_tests "/usr/bin/cmake" "-E" "env" "LATENCY_WORKER_BATCH=1" "LATENCY_TCP_NODELAY=1" "LATENCY_SERVER_BIN=/home/wensan/latency-lab/build-sanitize/latency_server" "/usr/bin/python3" "/home/wensan/latency-lab/tests/batch_server_tests.py")
set_tests_properties(batch_pipeline_tests PROPERTIES  _BACKTRACE_TRIPLES "/home/wensan/latency-lab/CMakeLists.txt;115;add_test;/home/wensan/latency-lab/CMakeLists.txt;0;")
add_test(concurrent_conservation_off "/usr/bin/cmake" "-E" "env" "LATENCY_TCP_NODELAY=1" "LATENCY_WORKER_BATCH=0" "LATENCY_SERVER_BIN=/home/wensan/latency-lab/build-sanitize/latency_server" "/usr/bin/python3" "/home/wensan/latency-lab/tests/concurrent_conservation_test.py")
set_tests_properties(concurrent_conservation_off PROPERTIES  _BACKTRACE_TRIPLES "/home/wensan/latency-lab/CMakeLists.txt;125;add_test;/home/wensan/latency-lab/CMakeLists.txt;0;")
add_test(concurrent_conservation_on "/usr/bin/cmake" "-E" "env" "LATENCY_TCP_NODELAY=1" "LATENCY_WORKER_BATCH=1" "LATENCY_SERVER_BIN=/home/wensan/latency-lab/build-sanitize/latency_server" "/usr/bin/python3" "/home/wensan/latency-lab/tests/concurrent_conservation_test.py")
set_tests_properties(concurrent_conservation_on PROPERTIES  _BACKTRACE_TRIPLES "/home/wensan/latency-lab/CMakeLists.txt;135;add_test;/home/wensan/latency-lab/CMakeLists.txt;0;")
