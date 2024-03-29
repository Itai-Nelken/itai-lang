cmake_minimum_required(VERSION 3.16)
project(ilc VERSION 0.1 LANGUAGES C)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
add_compile_options(-Wall -Wextra -Werror)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(sources
    src/Arena.c
    src/Array.c
    src/Ast.c
    src/Codegen.c
    src/Compiler.c
    src/Error.c
    src/memory.c
    src/Parser.c
    src/Scanner.c
    src/Table.c
    src/Token.c
    src/Types.c
    src/utilities.c
    src/Validator.c
    src/Strings.c
)

add_library(compiler OBJECT ${sources})

add_executable(ilc src/main.c)
target_link_libraries(ilc compiler)
add_executable(tests tests.c)
target_link_libraries(tests compiler)
