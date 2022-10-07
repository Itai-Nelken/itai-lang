cmake_minimum_required(VERSION 3.16)
project(icc VERSION 0.1 LANGUAGES C)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
add_compile_options(-Wall -Wextra)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(sources
    src/utilities.c
    src/Table.c
    src/Strings.c
    src/Array.c
    src/Token.c
    src/Ast.c
    src/Error.c
    src/Compiler.c
    src/Scanner.c
    src/Parser.c
)

add_library(compiler ${sources})

add_executable(ilc src/main.c)
target_link_libraries(ilc compiler)
add_executable(tests tests.c)
target_link_libraries(tests compiler)
