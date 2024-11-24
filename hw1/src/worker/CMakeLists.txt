add_library(integral-worker
  worker.cpp)

target_include_directories(integral-worker PUBLIC ${CMAKE_SOURCE_DIR})
