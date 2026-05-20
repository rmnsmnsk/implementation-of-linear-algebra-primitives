cmake_minimum_required(VERSION 3.10)
project(LinearAlgebraPrimitives)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(tests tests.c COO.c)
target_link_libraries(tests m)
