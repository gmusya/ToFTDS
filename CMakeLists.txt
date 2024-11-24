cmake_minimum_required(VERSION 3.22)
project(ToFTDS)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(GTest REQUIRED PATHS ${DEPS_PATH})
find_package(absl REQUIRED PATHS ${DEPS_PATH})

add_subdirectory(hw1)
