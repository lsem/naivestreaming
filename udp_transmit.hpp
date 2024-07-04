#pragma once

#include <asio/io_context.hpp>
#include <memory>
#include "types.hpp"


// TODO: How endpoints are going to find each other?
//   How to know they IPS?
class UDP_Transmit {
 public:
  virtual ~UDP_Transmit() = default;
  virtual void async_initialize(callback<void> cb) = 0;
  virtual void transmit(VideoPacket) = 0;
};

std::unique_ptr<UDP_Transmit> make_udp_transmit(asio::io_context&,
                                                std::string dest_host,
                                                int dest_port);
