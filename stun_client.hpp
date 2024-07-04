#pragma once
#include <asio/io_context.hpp>
#include <asio/ip/udp.hpp>
#include "types.hpp"

#include <string>

// STUN client solves us a problem of finding out our IP address in NAT server
// in front of our way before Internet. By resolving own IP address of our NAT
// we can share it with our peer.
class STUN_Client {
 public:
  virtual ~STUN_Client() = default;

  virtual void async_resolve_own_ip(callback<asio::ip::udp::endpoint> cb) = 0;
};

std::unique_ptr<STUN_Client> make_stun_client(asio::io_context& ctx,
                                              std::string stun_host);
