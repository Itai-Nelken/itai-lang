cmake_minimum_required(VERSION 3.16)
project(ilc VERSION 0.1 LANGUAGES C)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
add_compile_options(-Wall -Wextra -Werror)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(sources
    src/Arena.c
    src/Array.c
    src/Ast/ExprNode.c
    src/Ast/Module.c
    src/Ast/Object.c
    src/Ast/Program.c
    src/Ast/Scope.c
    src/Ast/StmtNode.c
    src/Ast/StringTable.c
    src/Ast/Type.c
    src/CGTargets/CCG.c
    src/Codegen.c
    src/Compiler.c
    src/Error.c
    src/memory.c
    src/Parser.c
    src/Scanner.c
    src/Table.c
    src/Token.c
    src/Typechecker.c
    src/utilities.c
    src/Validator.c
    src/Strings.c
)

add_library(compiler OBJECT ${sources})

add_executable(ilc src/main.c)
target_link_libraries(ilc compiler)
