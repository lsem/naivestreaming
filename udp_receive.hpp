#pragma once
#include <asio/io_context.hpp>
#include <memory>

#include "types.hpp"

class UDP_ReceiveListener {
 public:
  virtual ~UDP_ReceiveListener() = default;
  virtual void on_packet_received(VideoPacket p) = 0;
};

class UDP_Receive {
 public:
  virtual ~UDP_Receive() = default;
  virtual void start(UDP_ReceiveListener&) = 0;
};

std::unique_ptr<UDP_Receive> make_udp_receive(asio::io_context& ctx, int port);
