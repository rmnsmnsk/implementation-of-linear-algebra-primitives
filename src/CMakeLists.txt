cmake_minimum_required(VERSION 3.10)
project(SparseMatrixBenchmark C)

set(CMAKE_C_STANDARD 11)

add_library(c_impl_lib STATIC COO.c matrix.c)
target_include_directories(c_impl_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(../SuiteSparse/CSparse ${CMAKE_CURRENT_BINARY_DIR}/csparse)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(c_impl_lib PRIVATE -O3)
    target_compile_options(csparse PRIVATE -O3)
    target_compile_options(csparse_static PRIVATE -O3)
endif()

add_executable(tests tests.c)
target_link_libraries(tests PRIVATE c_impl_lib)

add_executable(benchmark benchmark.c)
target_link_libraries(benchmark PRIVATE c_impl_lib csparse_static m)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(benchmark PRIVATE -O3)
endif()
target_include_directories(benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../SuiteSparse/CSparse/Include)

if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    add_library(c_impl_profile_lib STATIC EXCLUDE_FROM_ALL COO.c matrix.c)
    target_include_directories(c_impl_profile_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_definitions(c_impl_profile_lib PRIVATE COO_PROFILE)
    target_compile_options(c_impl_profile_lib PRIVATE -O3 -g)

    add_executable(profile_benchmark EXCLUDE_FROM_ALL profile.c)
    target_compile_definitions(profile_benchmark PRIVATE COO_PROFILE)
    target_compile_options(profile_benchmark PRIVATE -O3 -g)
    target_link_libraries(profile_benchmark PRIVATE c_impl_profile_lib m)
endif()
