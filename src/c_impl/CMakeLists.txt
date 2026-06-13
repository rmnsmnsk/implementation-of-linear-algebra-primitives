add_library(c_impl_lib STATIC COO.c)

target_include_directories(c_impl_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(tests tests.c)
target_link_libraries(tests PRIVATE c_impl_lib)