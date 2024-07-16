cmake_minimum_required(VERSION 3.5)
project(naivestreaming)

set(CMAKE_CXX_STANDARD 20)
add_compile_options("-fdiagnostics-color=always")

add_subdirectory(vendor/asio)
add_subdirectory(vendor/expected)
add_subdirectory(vendor/googletest)
include(vendor/installed-ffmpeg-libs.cmake)
include(vendor/x264-superbuild.cmake)

add_subdirectory(src/libns)
add_subdirectory(src/stream_transmit)
add_subdirectory(src/stream_receive)
