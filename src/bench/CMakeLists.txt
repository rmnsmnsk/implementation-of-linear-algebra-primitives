cmake_minimum_required(VERSION 3.10)
project(benchmark_c C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -DNDEBUG")

add_executable(benchmark_c c_benchmark.c ../c_impl/COO.c)
target_link_libraries(benchmark_c m)