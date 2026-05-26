cmake_minimum_required(VERSION 3.10)
project(Bench C)

add_executable(benchmark_c c_benchmark.c ../c_impl/COO.c matrix_market.c)

target_include_directories(benchmark_c PRIVATE ../c_impl .)