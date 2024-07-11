cmake_minimum_required(VERSION 3.5)
project(naivestreaming)

set(CMAKE_CXX_STANDARD 20)

add_compile_options("-fdiagnostics-color=always")

add_subdirectory(vendor/asio)
add_subdirectory(vendor/edge264)
add_subdirectory(src/streamrender)

include(vendor/installed-ffmpeg-libs.cmake)
include(vendor/x264-superbuild.cmake)

set(SRC
  naive_streaming.cpp
  video_capture.cpp
  encoder.cpp
  decoder.cpp
  udp_transmit.cpp
  udp_receive.cpp
  stun_client.cpp
  log.cpp
)

add_executable(naivestreaming ${SRC})
target_compile_options(naivestreaming PUBLIC "-fsanitize=address")
target_link_options(naivestreaming PUBLIC "-fsanitize=address")
target_link_libraries( naivestreaming
  PUBLIC asio::asio
  PRIVATE libx264::libx264 edge264::edge264 ffmpeg::avfamily)
