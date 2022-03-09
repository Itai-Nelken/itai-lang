cmake_minimum_required(VERSION 3.16)
project(icc VERSION 0.1 LANGUAGES C)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(ilc src/main.c src/Token.c src/Scanner.c src/ast.c src/Parser.c)
