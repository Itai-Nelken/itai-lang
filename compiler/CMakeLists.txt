cmake_minimum_required(VERSION 3.16)
project(icc VERSION 0.1 LANGUAGES C)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
add_compile_options(-Wall -Wextra)

set(sources
    src/main.c
    src/utilities.c
    src/ast.c
    src/Token.c
    src/Scanner.c
    src/Parser.c
    src/codegen.c
    src/Table.c
    src/Strings.c
    src/Array.c
)

add_executable(ilc ${sources})
