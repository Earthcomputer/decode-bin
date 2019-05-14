cmake_minimum_required(VERSION 3.13)
project(decode-bin)

set(CMAKE_CXX_STANDARD 14)

add_executable(decode-bin
        src/decode_bin.cpp
        src/tokenizer.h
        src/tokenizer.cpp
        src/ast.h
        src/interpreter.h
        src/parser.h
        src/parser.cpp
        src/util.h
        src/util.cpp
        src/interpreter.cpp
        src/ast.cpp)
