cmake_minimum_required(VERSION 3.16)
project(ilc VERSION 0.1 LANGUAGES C)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
add_compile_options(-Wall -Wextra -Werror)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(sources
    src/Arena.c
    src/Array.c
    src/Ast.c
    src/Compiler.c
    src/Error.c
    src/memory.c
    src/Scanner.c
    src/Table.c
    src/Types.c
    src/Token.c
    src/utilities.c
    src/Strings.c
)

add_library(compiler OBJECT ${sources})

add_executable(ilc src/main.c)
target_link_libraries(ilc compiler)
