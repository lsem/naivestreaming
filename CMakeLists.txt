cmake_minimum_required(VERSION 3.5)
project(naivestreaming)

set(CMAKE_CXX_STANDARD 20)
add_compile_options("-fdiagnostics-color=always")

add_subdirectory(vendor/asio)
add_subdirectory(vendor/edge264)
add_subdirectory(src/streamrender)

include(vendor/installed-ffmpeg-libs.cmake)
include(vendor/x264-superbuild.cmake)

#########################################################
# ns_common library
add_library(ns_common log.hpp log.cpp)

#########################################################
# ns_encoder library
add_library(ns_encoder
  video_capture.hpp  
  video_capture.cpp
  encoder.cpp
  encoder.hpp
  udp_transmit.hpp    
  udp_transmit.cpp  
)
target_link_libraries(ns_encoder
  PUBLIC asio::asio PRIVATE ns_common libx264::libx264)

#########################################################
# ns_decoder library
set(ns_decoder_SRC
  decoder.cpp
  decoder.hpp    
  udp_receive.cpp
  udp_receive.hpp  
)
# Decoder part of the library
add_library(ns_decoder
  decoder.cpp  
  udp_receive.cpp
)
target_link_libraries(ns_decoder
  PUBLIC asio::asio PRIVATE ns_common ffmpeg::avfamily)


add_executable(naivestreaming naive_streaming.cpp)
target_compile_options(naivestreaming PUBLIC "-fsanitize=address")
target_link_options(naivestreaming PUBLIC "-fsanitize=address")
target_link_libraries(naivestreaming
  PUBLIC asio::asio
  PRIVATE ns_encoder ns_decoder)
