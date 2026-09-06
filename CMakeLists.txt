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

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(c_impl_lib PRIVATE -O3)
    target_compile_options(csparse PRIVATE -O3)
    target_compile_options(csparse_static PRIVATE -O3)
endif()

add_executable(tests ${SRC_DIR}/tests.c)
target_link_libraries(tests PRIVATE c_impl_lib)
target_include_directories(tests PRIVATE ${SRC_DIR})

add_executable(benchmark ${SRC_DIR}/benchmark.c)
target_link_libraries(benchmark PRIVATE c_impl_lib csparse_static m)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(benchmark PRIVATE -O3)
endif()
target_include_directories(benchmark PRIVATE
    ${SRC_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/SuiteSparse/CSparse/Include
)

if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    add_library(c_impl_profile_lib STATIC EXCLUDE_FROM_ALL
        ${SRC_DIR}/COO.c
        ${SRC_DIR}/matrix.c
    )
    target_include_directories(c_impl_profile_lib PUBLIC ${SRC_DIR})
    target_compile_definitions(c_impl_profile_lib PRIVATE COO_PROFILE)
    target_compile_options(c_impl_profile_lib PRIVATE -O3 -g)

    add_executable(profile_benchmark EXCLUDE_FROM_ALL ${SRC_DIR}/profile.c)
    target_compile_definitions(profile_benchmark PRIVATE COO_PROFILE)
    target_compile_options(profile_benchmark PRIVATE -O3 -g)
    target_link_libraries(profile_benchmark PRIVATE c_impl_profile_lib m)
endif()
