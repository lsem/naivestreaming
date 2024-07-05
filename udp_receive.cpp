#include "udp_receive.hpp"
#include <asio.hpp>

#include "log.hpp"

using asio::ip::udp;

class UDP_ReceiveImpl : public UDP_Receive {
 public:
  explicit UDP_ReceiveImpl(asio::io_context& ctx, int port)
      : m_ctx(ctx), m_port(port), m_socket(ctx) {}

  bool initialize() {
    std::error_code ec;

    m_socket.open(udp::v4(), ec);
    if (ec) {
      LOG_ERROR("Failed opening UDP socket: {}", ec.message());
      return false;
    }

    m_socket.bind(udp::endpoint(udp::v4(), m_port), ec);
    if (ec) {
      LOG_ERROR("Failed binding udp socket: {}", ec.message());
      return false;
    }

    LOG_DEBUG("bound to {}", m_port);

    // TODO: this is something I don't know yet if we can receive multiple
    // datagram at once.
    m_buffer.resize(1600);

    return true;
  }

  virtual void start(UDP_ReceiveListener&) override { receive_next(); }

  void receive_next() {
    m_socket.async_receive_from(
        asio::buffer(m_buffer), m_remote_endpoint, {},
        [this](std::error_code ec, size_t bytes_received) {
          if (ec) {
            LOG_ERROR("async_receive_from failed: {}", ec.message());
          } else {
            LOG_DEBUG("received {} bytes", bytes_received);
            // LOG_DEBUG("Endpoint: {}/{}",
            //           m_remote_endpoint.address().to_string(),
            //           m_remote_endpoint.port());
          }
          receive_next();
        });
    //    LOG_DEBUG("Ready to receive some data");
  }

 private:
  asio::io_context& m_ctx;
  int m_port{};
  udp::socket m_socket;
  udp::endpoint m_remote_endpoint;
  std::vector<uint8_t> m_buffer;
};

std::unique_ptr<UDP_Receive> make_udp_receive(asio::io_context& ctx, int port) {
  auto instance = std::make_unique<UDP_ReceiveImpl>(ctx, port);
  if (!instance->initialize()) {
    LOG_ERROR("Failed to initialize UDP_Receive");
    return nullptr;
  }
  return instance;
}
