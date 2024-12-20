cmake_minimum_required(VERSION 3.22)
project(ToFTDS)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(GTest REQUIRED PATHS ${DEPS_PATH})
find_package(absl REQUIRED PATHS ${DEPS_PATH})

find_package(Protobuf REQUIRED PATHS ${DEPS_PATH})
set(_PROTOBUF_LIBPROTOBUF protobuf::libprotobuf)
set(_REFLECTION gRPC::grpc++_reflection)
set(_PROTOBUF_PROTOC $<TARGET_FILE:protobuf::protoc>)

find_package(gRPC REQUIRED PATHS ${DEPS_PATH})
set(_GRPC_GRPCPP gRPC::grpc++)
set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)

add_subdirectory(hw1)
# add_subdirectory(hw2)

add_subdirectory(hw3)
