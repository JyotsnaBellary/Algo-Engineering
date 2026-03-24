cmake_minimum_required(VERSION 3.16)

project(MultiwayCut)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenMP REQUIRED)

add_executable(multiwaycut
    benchmark/benchmark.cpp
    benchmark/test.cpp
    algorithms/maximum_flow.cpp
    data_structure/graph.cpp
    utils/file_handler.cpp
    algorithms/heuristic.cpp
    algorithms/parallel_heuristic.cpp
    algorithms/approximation.cpp
    algorithms/exact.cpp
    benchmark/benchmark.cpp
)

target_include_directories(multiwaycut PRIVATE
    /usr/local/include
    include
    data_structure
    algorithms
    utils
    benchmark
    input_data
)

target_link_directories(multiwaycut PRIVATE /usr/local/lib)

target_link_libraries(multiwaycut PRIVATE glpk OpenMP::OpenMP_CXX)
