cmake_minimum_required (VERSION 2.6)
project (lisp_test C)

if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pedantic -Wall -g")
endif()

add_library(lisp parse.c eval.c)
add_executable(lisp_test test.c)
target_link_libraries(lisp_test lisp)
