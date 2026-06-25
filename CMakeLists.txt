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
    src/baseline/brute_force.cpp
    src/baseline/yen.cpp
    src/two_vdp/two_vdp.cpp
    src/cwz/reductions.cpp
    src/cwz/nsp.cpp
    generators/generators.cpp
)
target_include_directories(cwz_core PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR})

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
    tests/unit/test_brute_force.cpp
    tests/unit/test_yen.cpp
    tests/unit/test_generators.cpp
    tests/unit/test_two_vdp.cpp
    tests/unit/test_reductions.cpp
    tests/unit/test_cwz_nsp.cpp
)
target_link_libraries(unit_tests PRIVATE cwz_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(unit_tests)

add_executable(fuzz_brute_vs_yen tests/correctness/fuzz_brute_vs_yen.cpp)
target_link_libraries(fuzz_brute_vs_yen PRIVATE cwz_core)

add_executable(fuzz_cwz_vs_brute tests/correctness/fuzz_cwz_vs_brute.cpp)
target_link_libraries(fuzz_cwz_vs_brute PRIVATE cwz_core)

add_executable(fuzz_cwz_vs_yen tests/correctness/fuzz_cwz_vs_yen.cpp)
target_link_libraries(fuzz_cwz_vs_yen PRIVATE cwz_core)

add_executable(profile_cwz tests/correctness/profile_cwz.cpp)
target_link_libraries(profile_cwz PRIVATE cwz_core)

add_executable(inspect_slow tests/correctness/inspect_slow.cpp)
target_link_libraries(inspect_slow PRIVATE cwz_core)

add_executable(bench bench/bench.cpp)
target_link_libraries(bench PRIVATE cwz_core)

add_executable(bench_adversarial bench/bench_adversarial.cpp)
target_link_libraries(bench_adversarial PRIVATE cwz_core)

add_executable(nsp-cli src/cli/main.cpp)
target_link_libraries(nsp-cli PRIVATE cwz_core)
