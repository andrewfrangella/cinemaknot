cmake_minimum_required(VERSION 3.10)
project(CinemaKnot VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

# Test executable
add_executable(knot_test examples/test_knot.cpp)

# Pi Cinema Node
add_executable(pi_cinema_node examples/pi_cinema_node.cpp)

# Target compile options
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(knot_test PRIVATE -Wall -Wextra -O3)
    target_compile_options(pi_cinema_node PRIVATE -Wall -Wextra -O3)
endif()
