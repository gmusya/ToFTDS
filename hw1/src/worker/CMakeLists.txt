add_library(integral-worker
  worker.cpp)

target_include_directories(integral-worker PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(integral-worker PUBLIC integral-socket integral-common)

add_executable(integral-worker-launch
  worker_launch.cpp)
target_link_libraries(integral-worker-launch integral-worker absl::flags absl::flags_parse)
