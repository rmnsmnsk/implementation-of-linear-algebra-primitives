cmake_minimum_required(VERSION 3.10)
project(SparseMatrixBenchmark C)

set(CMAKE_C_STANDARD 11)

add_library(c_impl_lib STATIC COO.c matrix.c)
target_include_directories(c_impl_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(../SuiteSparse/CSparse ${CMAKE_CURRENT_BINARY_DIR}/csparse)

add_executable(tests tests.c)
target_link_libraries(tests PRIVATE c_impl_lib)

add_executable(benchmark benchmark.c)
target_link_libraries(benchmark PRIVATE c_impl_lib csparse m)
target_include_directories(benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../SuiteSparse/CSparse/Include)