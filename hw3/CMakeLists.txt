find_package(cpprestsdk REQUIRED)

add_executable(server server.cpp)
target_link_libraries(server PRIVATE cpprestsdk::cpprest crypto)
target_include_directories(server PUBLIC ${CMAKE_SOURCE_DIR})
