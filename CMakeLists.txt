cmake_minimum_required(VERSION 3.26)
project(cpp_position_independent_data)

include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Werror")

enable_testing()

add_executable(tests test.cpp test-build-datastructures.cpp test_main.cpp)

add_test(tests tests)
