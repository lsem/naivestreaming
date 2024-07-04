#include "udp_transmit.hpp"
#include <asio.hpp>
#include "log.hpp"

using asio::ip::udp;

#define CMOVE(X) X = std::move(X)

class UDP_TransmitImpl : public UDP_Transmit {
 public:
  explicit UDP_TransmitImpl(asio::io_context& ctx,
                            std::string dest_host,
                            int port)
      : m_ctx(ctx), m_dest_host(dest_host), m_port(port), m_socket(ctx) {}

  bool initialize() {
    std::error_code ec;

    m_socket.open(udp::v4(), ec);
    if (ec) {
      LOG_ERROR("Failed opening transmit socket: {}", ec.message());
      return false;
    }

    // Settings non-blocking mode on a socket to be able to use send_to in
    // non-blocking fashion.
    // m_socket.non_blocking(true, ec);
    // if (ec) {
    //   LOG_ERROR("Failed settings non-blocking mode on a socket: {}",
    //             ec.message());
    //   return false;
    // }

    return true;
  }

  virtual void async_initialize(callback<void> cb) override {
    cb({});
    // m_resolver.async_resolve(
    //     udp::v4(), m_dest_host, std::to_string(m_dest_port),
    //     [CMOVE(cb), this](
    //         std::error_code ec,
    //         asio::ip::basic_resolver_results<asio::ip::udp> results) {
    //       if (ec) {
    //         LOG_ERROR("async_resolve failed: {}", ec.message());
    //         cb(ec);
    //         return;
    //       }
    //       LOG_DEBUG("async_resolve returned ({} results)", results.size());
    //       if (!results.empty()) {
    //         m_endpoint = *results.begin();
    //         cb({});
    //       } else {
    //         cb(make_error_code(std::errc::io_error));
    //       }
    //     });
  }

  virtual void transmit(VideoPacket packet) override {
    // TODO: pack into actual RTP-like packet.
    std::vector<asio::const_buffer> buffers{asio::buffer(packet.nal_data)};
    std::error_code ec;
    udp::endpoint endpoint{asio::ip::address::from_string("127.0.0.1"), m_port};
    m_socket.send_to(buffers, endpoint, {}, ec);
    if (ec) {
      if (ec != asio::error::would_block) {
        LOG_WARNING("Failed sending packet: {}", ec.message());
      } else {
        LOG_DEBUG("Buffer stalled");
      }
    } else {
      LOG_DEBUG("packet sent to port {}", m_port);
    }
  }

 private:
  asio::io_context& m_ctx;
  std::string m_dest_host;
  int m_port{};
  udp::socket m_socket;
  udp::endpoint m_endpoint;
  udp::resolver m_resolver{m_ctx};
};

std::unique_ptr<UDP_Transmit> make_udp_transmit(asio::io_context& ctx,
                                                std::string dest_host,
                                                int port) {
  auto instance = std::make_unique<UDP_TransmitImpl>(ctx, dest_host, port);
  if (!instance->initialize()) {
    LOG_ERROR("Failed initializing UDP_Transmit");
    return nullptr;
  }

  return instance;
}