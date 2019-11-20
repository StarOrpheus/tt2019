project(lab0)

cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)

include_directories("include")

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    set(CMAKE_CXX_FLAGS "-pthread ${CMAKE_CXX_FLAGS}")
endif()

add_library(lambdas STATIC
                lambdas.cpp include/lambdas.h)

add_executable(task0 task0.cpp)
target_link_libraries(task0 lambdas)
