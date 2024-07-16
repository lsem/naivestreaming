#########################################################
# ns_common library
add_library(ns_common log.hpp log.cpp types.hpp rtp.hpp rtp.cpp defs.hpp)
add_library(ns::common ALIAS ns_common)
target_include_directories(ns_common PUBLIC .)
target_link_libraries(ns_common PUBLIC tl::expected)

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
add_library(ns::encoder ALIAS ns_encoder)
target_include_directories(ns_encoder PUBLIC .)
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
add_library(ns::decoder ALIAS ns_decoder)
target_include_directories(ns_decoder PUBLIC .)
target_link_libraries(ns_decoder
  PUBLIC asio::asio PRIVATE ns_common ffmpeg::avfamily)

add_executable(ns_tests tests/rtp_tests.cpp)
target_link_libraries(ns_tests
  PRIVATE GTest::gtest GTest::gtest_main ns::common ns::encoder ns::decoder)
