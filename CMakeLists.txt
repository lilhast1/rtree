cmake_minimum_required(VERSION 3.14)
project(RTreeComparison LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default to Debug build
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# ================================
# Sanitizers and warnings per compiler
# ================================

if(MSVC)
    message(STATUS "Configuring for MSVC — enabling AddressSanitizer")
    add_compile_options(/W4 /fsanitize=address)
    add_link_options(/fsanitize=address)
else()
    message(STATUS "Configuring for GCC/Clang — enabling ASan + UBSan")
    add_compile_options(-Wall -Wextra -Wpedantic -g -O2)
    add_compile_options(-fsanitize=address -fsanitize=undefined)
    add_link_options(-fsanitize=address -fsanitize=undefined)
endif()
# ------------------------
# Catch2 for unit tests
# ------------------------
include(FetchContent)
FetchContent_Declare(
  catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.6.0
)
FetchContent_MakeAvailable(catch2)

# ------------------------
# Library target
# ------------------------
file(GLOB_RECURSE RTREE_SOURCES
    src/rtree_hilbert/*.cpp
    src/rtree/*.cpp
)

add_library(rtree_lib STATIC ${RTREE_SOURCES})
target_include_directories(rtree_lib PUBLIC src include)

# ------------------------
# Main executable (benchmark / comparison)
# ------------------------
add_executable(benchmark src/main.cpp)
target_link_libraries(benchmark PRIVATE rtree_lib)

# ------------------------
# Tests
# ------------------------
file(GLOB_RECURSE TEST_SOURCES tests/*.cpp)

add_executable(tests ${TEST_SOURCES})
target_link_libraries(tests PRIVATE rtree_lib Catch2::Catch2WithMain)
target_include_directories(tests PRIVATE src include)

enable_testing()
add_test(NAME RTreeTests COMMAND tests)
