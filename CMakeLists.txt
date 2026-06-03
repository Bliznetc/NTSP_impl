cmake_minimum_required(VERSION 3.20)
project(cwz_nsp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

include_directories(${CMAKE_SOURCE_DIR}/src)

add_library(cwz_core STATIC
    src/graph/graph.cpp
    src/shortest_path/dijkstra.cpp
    src/shortest_path/sp_dag.cpp
)
target_include_directories(cwz_core PUBLIC ${CMAKE_SOURCE_DIR}/src)

enable_testing()

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(unit_tests
    tests/unit/test_graph.cpp
    tests/unit/test_dijkstra.cpp
    tests/unit/test_sp_dag.cpp
)
target_link_libraries(unit_tests PRIVATE cwz_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(unit_tests)
