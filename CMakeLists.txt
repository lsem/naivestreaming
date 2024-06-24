project(naivestreaming)
cmake_minimum_required(VERSION 3.5)

add_compile_options("-fdiagnostics-color=always")

set(SRC naive_streaming.cpp)

add_executable(naivestreaming ${SRC})
