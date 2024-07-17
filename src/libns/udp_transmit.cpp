#include "udp_transmit.hpp"
#include <asio.hpp>
#include <chrono>
#include "log.hpp"
#include "rtp.hpp"

LOG_MODULE_NAME("UDP_TX");

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

    return true;
  }

  virtual void async_initialize(callback<void> cb) override { cb({}); }

  virtual void transmit(VideoPacket packet) override {
    RTP_PacketHeader header;
    header.version = 2;
    header.padding_bit = 1;
    header.extension_bit = 0;
    header.marker_bit = 0;
    header.payload_type = 78;
    header.sequence_num = m_sequence_num++;

    // 2^32 milliseconds is ~49 days. As long as we are interested only in
    // difference between consecutive packets we should be fine.
    header.timestamp = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            packet.timestamp - std::chrono::steady_clock::time_point{})
            .count());

    // TODO: encode first_macroblock and last_macroblock into a packet.
    // Probably, we can use extension for this purpose or additional payload
    // header.

    std::array<uint8_t, RTP_PacketHeader_Size> header_buff;

    if (auto ec = serialize_rtp_header_to(header, header_buff); ec) {
      LOG_ERROR("Failed serializing packet, ignoring: {}", ec.message());
      return;
    }

    LOG_DEBUG("header_buff[0]: {}", header_buff[0]);

    std::vector<asio::const_buffer> buffers{asio::buffer(header_buff),
                                            asio::buffer(packet.nal_data)};
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
      LOG_DEBUG("packet of size {} sent to port {}", packet.nal_data.size(),
                m_port);
    }
  }

 private:
  asio::io_context& m_ctx;
  std::string m_dest_host;
  int m_port{};
  udp::socket m_socket;
  udp::endpoint m_endpoint;
  udp::resolver m_resolver{m_ctx};
  std::atomic<unsigned> m_sequence_num{};
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
