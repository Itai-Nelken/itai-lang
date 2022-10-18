cmake_minimum_required(VERSION 3.16)
project(ir VERSION 0.1 LANGUAGES C)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
add_compile_options(-Wall -Wextra)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(sources
    src/vm.c
)

add_library(ir OBJECT ${sources})

add_executable(ir_test main.c)
target_link_libraries(ir_test ir)
