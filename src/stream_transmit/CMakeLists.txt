add_executable(stream_transmit stream_transmit_main.cpp)
target_compile_options(stream_transmit PUBLIC "-fsanitize=address")
target_link_options(stream_transmit PUBLIC "-fsanitize=address")
target_link_libraries(stream_transmit PRIVATE asio::asio ns::encoder)
