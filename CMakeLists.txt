cmake_minimum_required(VERSION 3.16)
project(lob_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-O2 -Wall -Wextra -Wpedantic)

include_directories(${CMAKE_SOURCE_DIR}/include)

# Replay demo
add_executable(lob_replay src/main.cpp)

# Benchmark
add_executable(lob_bench benchmark/bench.cpp)

# Tests
enable_testing()
add_executable(lob_tests tests/test_order_book.cpp)
add_test(NAME order_book_tests COMMAND lob_tests)
