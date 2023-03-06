cmake_minimum_required(VERSION 3.16)
project(ir VERSION 0.1 LANGUAGES C)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
add_compile_options(-Wall -Wextra -Werror)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(sources
    src/main.c
)

add_executable(ir_test ${sources})
