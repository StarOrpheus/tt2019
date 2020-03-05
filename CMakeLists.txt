project(TT2019)

cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)

include_directories("include")

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    set(CMAKE_CXX_FLAGS "-pthread ${CMAKE_CXX_FLAGS}")
endif()

add_subdirectory(3rd-party/)

add_executable(parser_testing parser_testing.cpp)
target_link_libraries(parser_testing gtest)

add_executable(task0 task0.cpp)
add_executable(task2 task2.cpp)
add_executable(lambda_to_dot vis.cpp)
add_executable(diff_outputs diff_outs.cpp)