add_library(rsm-consensus
  node.cpp)

target_include_directories(rsm-consensus PUBLIC ${CMAKE_SOURCE_DIR})

add_executable(rsm-consensus-ut
  ut/node_test.cpp
)
target_link_libraries(rsm-consensus-ut PUBLIC rsm-consensus GTest::gtest_main)
