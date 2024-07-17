#include "udp_receive.hpp"
#include <asio.hpp>

#include "log.hpp"
#include "rtp.hpp"

LOG_MODULE_NAME("UDP_RECEIVE");

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

  virtual void start(UDP_ReceiveListener& listener) override {
    m_listener = &listener;
    receive_next();
  }

  void receive_next() {
    assert(m_listener != nullptr);

    m_socket.async_receive_from(
        asio::buffer(m_buffer), m_remote_endpoint, {},
        [this](std::error_code ec, size_t bytes_received) {
          if (ec) {
            LOG_ERROR("async_receive_from failed: {}", ec.message());
          } else {
            LOG_DEBUG("received {} bytes", bytes_received);

            // TODO: I doubt that just a jeader is enough. There must be more
            // realistic minimum packet size.
            // TODO: check if at least version looks good before parsing
            // potential crap.
            if (bytes_received < 12) {
              // TODO: count this events and remove logging and just ignore.
              LOG_ERROR("Got too small packet");
              // TODO: refactor in a way that we don't need to write
              // receive_next().
              receive_next();
              return;
            }
            auto maybe_rtp_header = deserialize_rtp_header_from(m_buffer);
            if (!maybe_rtp_header.has_value()) {
              LOG_ERROR("Got data that cannot be RTP header: {}",
                        maybe_rtp_header.error().message());
              receive_next();
              return;
            }
            auto& rtp_header = *maybe_rtp_header;

            if (rtp_header.version != 2) {
              // TODO: should be removed from production, just count.
              LOG_DEBUG("Wrong RTP packet, wrong version: {}",
                        rtp_header.version);
              receive_next();
              return;
            }

            if (rtp_header.extension_bit) {
              // We don't support extensions at the moment. If there was an
              // extension we would need to change calculation of payload begin.
              // TODO: this warning should be removed from production.
              LOG_WARNING("Packet with extension bit set, ignoring..");
              receive_next();
              return;
            }

            std::span<const uint8_t> payload{
                m_buffer.begin() + RTP_PacketHeader_Size, m_buffer.end()};

            LOG_DEBUG("Got something that looks like a header");

            VideoPacket packet{.nal_data = std::vector<uint8_t>{payload.begin(),
                                                                payload.end()}};

            // TODO: packets should be reordered by sequence level. There should
            // also be a timeout.

            m_listener->on_packet_received(std::move(packet));
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
  UDP_ReceiveListener* m_listener{};
};

std::unique_ptr<UDP_Receive> make_udp_receive(asio::io_context& ctx, int port) {
  auto instance = std::make_unique<UDP_ReceiveImpl>(ctx, port);
  if (!instance->initialize()) {
    LOG_ERROR("Failed to initialize UDP_Receive");
    return nullptr;
  }
  return instance;
}
