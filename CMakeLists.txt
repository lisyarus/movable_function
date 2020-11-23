cmake_minimum_required(VERSION 3.10)
project(movable_function CXX)

set(CMAKE_CXX_STANDARD 17)

add_executable(bench bench.cpp function.hpp)
