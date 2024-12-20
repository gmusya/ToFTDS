add_subdirectory(broadcast)

get_filename_component(communication_proto communication.proto ABSOLUTE)

set(communication_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/communication.pb.cc")
set(communication_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/communication.pb.h")
set(communication_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/communication.grpc.pb.cc")
set(communication_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/communication.grpc.pb.h")
if(NOT Protobuf_INCLUDE_DIRS)
  set(Protobuf_INCLUDE_DIRS ${CMAKE_SOURCE_DIR})
endif()
add_custom_command(
  OUTPUT "${communication_proto_srcs}" "${communication_proto_hdrs}" "${communication_grpc_srcs}"
         "${communication_grpc_hdrs}"
  COMMAND
    ${_PROTOBUF_PROTOC} ARGS --grpc_out "${CMAKE_BINARY_DIR}" --cpp_out
    "${CMAKE_BINARY_DIR}" -I "${Protobuf_INCLUDE_DIRS}" -I "${CMAKE_SOURCE_DIR}"
    --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}" "${communication_proto}"
  DEPENDS "${communication_proto}")

# communication_grpc_proto
add_library(communication_grpc_proto ${communication_grpc_srcs} ${communication_grpc_hdrs}
                              ${communication_proto_srcs} ${communication_proto_hdrs})
target_link_libraries(communication_grpc_proto ${_REFLECTION} ${_GRPC_GRPCPP}
                      ${_PROTOBUF_LIBPROTOBUF})
target_include_directories(communication_grpc_proto PUBLIC ${CMAKE_BINARY_DIR})

set_target_properties(communication_grpc_proto PROPERTIES POSITION_INDEPENDENT_CODE ON)

find_package(cpprestsdk REQUIRED)

add_executable(server server.cpp)
target_link_libraries(server PRIVATE communication_grpc_proto cpprestsdk::cpprest crypto absl::flags absl::flags_parse absl::log)
target_include_directories(server PUBLIC ${CMAKE_SOURCE_DIR})
