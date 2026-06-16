cmake_minimum_required(VERSION 3.10)
project(SparseMatrixBenchmark C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(c_impl_lib STATIC
    ${SRC_DIR}/COO.c
    ${SRC_DIR}/matrix.c
)

target_include_directories(c_impl_lib PUBLIC ${SRC_DIR})

add_subdirectory(SuiteSparse/CSparse ${CMAKE_CURRENT_BINARY_DIR}/csparse)

add_executable(tests ${SRC_DIR}/tests.c)
target_link_libraries(tests PRIVATE c_impl_lib)
target_include_directories(tests PRIVATE ${SRC_DIR})

add_executable(benchmark ${SRC_DIR}/benchmark.c)
target_link_libraries(benchmark PRIVATE c_impl_lib csparse m)
target_include_directories(benchmark PRIVATE
    ${SRC_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/SuiteSparse/CSparse/Include
)