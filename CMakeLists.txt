cmake_minimum_required(VERSION 3.15)
project(kvstore)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")

file(GLOB_RECURSE SOURCES "store/*.cpp" "server/*.cpp" "aof.cpp" "main.cpp")

add_executable(kvstore ${SOURCES})
target_include_directories(kvstore PRIVATE ${CMAKE_SOURCE_DIR})

find_package(Threads REQUIRED)
target_link_libraries(kvstore PRIVATE Threads::Threads)