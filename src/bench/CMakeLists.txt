cmake_minimum_required(VERSION 3.10)
project(Bench C)

add_executable(benchmark_c c_benchmark.c matrix_market.c)

target_include_directories(benchmark_c PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../c_impl)

target_link_libraries(benchmark_c PRIVATE c_impl_lib)