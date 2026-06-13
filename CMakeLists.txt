cmake_minimum_required(VERSION 3.10)
project(LinearAlgebraPrimitives C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O2")

add_subdirectory(src/c_impl)
add_subdirectory(src/bench)