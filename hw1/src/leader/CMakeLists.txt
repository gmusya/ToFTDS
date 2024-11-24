add_library(integral-leader
  leader.cpp)

target_include_directories(integral-leader PUBLIC ${CMAKE_SOURCE_DIR})

add_executable(integral-leader-launch
  leader_launch.cpp)
target_link_libraries(integral-leader-launch integral-leader absl::flags absl::flags_parse)
