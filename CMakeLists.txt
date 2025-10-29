cmake_minimum_required(VERSION 3.14)
project(HilbertRTreeComparison LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17
)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

add_compile_options(-Wall -Wextra -Wpedantic -O2)

file(GLOB_RECURSE SOURCES src/*.cpp)
add_executable(hilbert_rtree ${SOURCES})
target_include_directories(hilbert_rtree PRIVATE include)
