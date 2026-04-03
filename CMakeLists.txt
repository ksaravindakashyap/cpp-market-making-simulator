cmake_minimum_required(VERSION 3.20)
project(cpp-market-making-simulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MMSIM_BUILD_SERVER "Build the simulation server executable" ON)
option(MMSIM_BUILD_BENCH "Build benchmarks" ON)
option(MMSIM_BUILD_TESTS "Build unit tests" ON)

add_subdirectory(core)

if(MMSIM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(MMSIM_BUILD_SERVER)
    add_subdirectory(server)
endif()

if(MMSIM_BUILD_BENCH)
    add_subdirectory(bench)
endif()
