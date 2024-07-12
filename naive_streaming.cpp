
#include <asio/io_context.hpp>
#include <asio/signal_set.hpp>

#include "encoder.hpp"
#include "log.hpp"
#include "types.hpp"
#include "udp_receive.hpp"
#include "udp_transmit.hpp"
#include "video_capture.hpp"

using namespace std;

/* **SUBTASKS**
 *   1) component responsible for capturing video (getting a constant flow of
 * image frames) 2) component for encoding captrured video stream. 3) Component
 * for sending video stream. 4) Component for receiving video stream. 5)
 * Component for decoding video stream. 6) Component for displaying video
 * stream.
 */

// What is a challange of implementing RTP/RTCP having only ASIO.
// Not only it should give us some

class Application : public EncoderClient {
 public:
  explicit Application(asio::io_context& ctx) : m_ctx(ctx) {}

  bool initialize() {
    m_encoder = make_encoder(*this);
    if (!m_encoder) {
      LOG_ERROR("Failed creating encoder");
      return false;
    }

    auto devs = enumerate_video4_linux_devices();
    if (devs.empty()) {
      LOG_ERROR("No v4l2 devices found");
      return false;
    }
    LOG_DEBUG("Video4Linux devices:");
    for (auto& x : devs) {
      cout << x << "\n";
    }

    m_capture = make_video_capture(devs[0], [this](BufferView buff) {
      // WARNING: called from other thread!
      m_encoder->process_frame(buff);
    });
    if (!m_capture) {
      LOG_ERROR("Failed creating videocapture");
      return -1;
    }

    m_capture->print_capabilities();

    LOG_DEBUG("Available formats:");
    auto formats = m_capture->enumerate_formats();
    if (formats.empty()) {
      LOG_ERROR("No available video formats");
      return -1;
    }
    // TODO: find format we really want and need instead of random last one.
    m_capture->select_format(*formats.back());

    constexpr int port = 34000;

    m_udp_transmit = make_udp_transmit(m_ctx, "127.0.0.1", port);
    if (!m_udp_transmit) {
      LOG_ERROR("Failed creating UDP transmit");
      return false;
    }

    // TODO: this should actually be bound to some interface, not to just a
    // port.
    // m_udp_receive = make_udp_receive(m_ctx, port);
    // if (!m_udp_receive) {
    //   LOG_ERROR("Failed creating UDP receive");
    //   return false;
    // }

    return true;
  }

  virtual void on_frame_started() override {
    LOG_DEBUG("Application: Frame started");
  }

  virtual void on_frame_ended() override {
    LOG_DEBUG("Application: Frame finished");
  }

  virtual void on_nal_encoded(const uint8_t* data, size_t data_size) override {
    //    LOG_DEBUG("Application: sending NAL over UDP");
    VideoPacket packet;
    packet.nal_data.assign(data, data + data_size);
    m_udp_transmit->transmit(std::move(packet));
    // m_decoder->decode_packet(packet);
    // TODO: theoretically, by utilizing scatter-gather IO we can eliminate
    // copying here completely.
  }

  void async_start_streaming(callback<void> cb) {
    // TODO: we cab start capturing veideostream in parallel to resolve.
    LOG_INFO("Starting Streaming..");
    m_udp_transmit->async_initialize([cb = std::move(cb), this](auto ec) {
      if (ec) {
        LOG_ERROR("Failed initializing UDP transmit: {}", ec.message());
        cb(ec);
        return;
      }

      // LOG_DEBUG("UDP transmit initialized, starting streaming...");
      // m_udp_receive->start(*this);

      m_capture->start();
      cb({});
    });
  }

  void stop() { m_capture->stop(); }

 private:
  asio::io_context& m_ctx;
  std::unique_ptr<Encoder> m_encoder;
  std::unique_ptr<VideoCapture> m_capture;
  std::unique_ptr<UDP_Transmit> m_udp_transmit;
  //  std::unique_ptr<UDP_Receive> m_udp_receive;
};

int main() {
  asio::io_context ctx;

  Application app{ctx};
  if (!app.initialize()) {
    LOG_ERROR("Failed initializating app. Exiting..");
    return -1;
  }

  app.async_start_streaming([](auto ec) {
    if (ec) {
      LOG_ERROR("Failed starting streaming: {}", ec.message());
      std::exit(1);
    }
  });

  asio::signal_set signals{ctx, SIGTERM, SIGINT};
  signals.async_wait([&app, &ctx](std::error_code ec, int signal) {
    if (ec) {
      LOG_ERROR("Error in signals handler");
      std::quick_exit(1);
      return;
    }
    LOG_DEBUG("Got signal {}", signal);
    // TODO: stop needs to accept timeout.
    app.stop();
    ctx.stop();
  });

  asio::io_context::work w{ctx};
  LOG_INFO("Running event loop ");
  ctx.run();
  LOG_INFO("Event loop has stopped");
}
