add_executable(cli
  cli.cpp)

target_link_libraries(cli kv_store_grpc_proto absl::flags absl::flags_parse)
target_include_directories(cli PUBLIC ${CMAKE_SOURCE_DIR})
