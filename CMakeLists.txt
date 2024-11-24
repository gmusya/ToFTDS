cmake_minimum_required(VERSION 3.22)
project(ToFTDS)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(GTest REQUIRED PATHS ${GTEST_ROOT})

add_subdirectory(hw1)
