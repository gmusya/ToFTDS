get_filename_component(kv_store_proto kv_store.proto ABSOLUTE)

set(kv_store_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/kv_store.pb.cc")
set(kv_store_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/kv_store.pb.h")
set(kv_store_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/kv_store.grpc.pb.cc")
set(kv_store_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/kv_store.grpc.pb.h")
if(NOT Protobuf_INCLUDE_DIRS)
  set(Protobuf_INCLUDE_DIRS ${CMAKE_SOURCE_DIR})
endif()
add_custom_command(
  OUTPUT "${kv_store_proto_srcs}" "${kv_store_proto_hdrs}" "${kv_store_grpc_srcs}"
         "${kv_store_grpc_hdrs}"
  COMMAND
    ${_PROTOBUF_PROTOC} ARGS --grpc_out "${CMAKE_BINARY_DIR}" --cpp_out
    "${CMAKE_BINARY_DIR}" -I "${Protobuf_INCLUDE_DIRS}" -I "${CMAKE_SOURCE_DIR}"
    --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}" "${kv_store_proto}"
  DEPENDS "${kv_store_proto}")

# kv_store_grpc_proto
add_library(kv_store_grpc_proto ${kv_store_grpc_srcs} ${kv_store_grpc_hdrs}
                              ${kv_store_proto_srcs} ${kv_store_proto_hdrs})
target_link_libraries(kv_store_grpc_proto ${_REFLECTION} ${_GRPC_GRPCPP}
                      ${_PROTOBUF_LIBPROTOBUF})
target_include_directories(kv_store_grpc_proto PUBLIC ${CMAKE_BINARY_DIR})

set_target_properties(kv_store_grpc_proto PROPERTIES POSITION_INDEPENDENT_CODE ON)


add_executable(worker_launch
  worker.cpp)

target_link_libraries(worker_launch rsm-consensus kv_store_grpc_proto absl::flags absl::flags_parse)
target_include_directories(worker_launch PUBLIC ${CMAKE_SOURCE_DIR})
