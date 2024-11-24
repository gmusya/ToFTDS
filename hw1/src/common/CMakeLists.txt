add_library(integral-common
  integral.cpp)

target_include_directories(integral-common PUBLIC ${CMAKE_SOURCE_DIR})

add_executable(integral-common-ut
  ut/integral_test.cpp
)
target_link_libraries(integral-common-ut PUBLIC integral-common GTest::gtest_main)
