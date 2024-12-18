add_executable(broadcast-ut
  ut/node_test.cpp
)
target_link_libraries(broadcast-ut PUBLIC GTest::gtest_main)
target_include_directories(broadcast-ut PUBLIC ${CMAKE_SOURCE_DIR})
