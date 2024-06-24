cmake_minimum_required(VERSION 3.5)
project(naivestreaming)

add_compile_options("-fdiagnostics-color=always")

add_subdirectory(vendor/asio)

set(SRC naive_streaming.cpp)
add_executable(naivestreaming ${SRC})
target_link_libraries(naivestreaming PUBLIC asio::asio)
target_compile_options(naivestreaming PUBLIC "-fsanitize=address")
target_link_options(naivestreaming PUBLIC "-fsanitize=address")
