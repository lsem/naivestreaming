
#include <chrono>

#include <asio/io_context.hpp>
#include <asio/signal_set.hpp>

#include "encoder.hpp"
#include "log.hpp"
#include "types.hpp"
#include "udp_receive.hpp"
#include "udp_transmit.hpp"
#include "video_capture.hpp"

LOG_MODULE_NAME("TNSM_APP")

using namespace std;

class StreamTransmitApp : public EncoderClient {
 public:
  explicit StreamTransmitApp(asio::io_context& ctx) : m_ctx(ctx) {}

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

    m_capture = make_video_capture(devs[0], [this](std::span<uint8_t> data) {
      // WARNING: called from other thread!
      const auto ts = std::chrono::steady_clock::now();
      m_encoder->process_frame(data, CapturedFrameMeta{.timestamp = ts});
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

    return true;
  }

  virtual void on_frame_started() override {
    LOG_DEBUG("Application: Frame started");
  }

  virtual void on_frame_ended() override {
    LOG_DEBUG("Application: Frame finished");
  }

  virtual void on_nal_encoded(std::span<const uint8_t> data,
                              EncodedFrameMetadata meta) override {
    VideoPacket packet;
    packet.nal_data.assign(data.begin(), data.end());
    packet.timestamp = meta.timestamp;
    m_udp_transmit->transmit(std::move(packet));
  }

  void async_start_streaming(callback<void> cb) {
    LOG_INFO("Starting Streaming..");
    m_udp_transmit->async_initialize([cb = std::move(cb), this](auto ec) {
      if (ec) {
        LOG_ERROR("Failed initializing UDP transmit: {}", ec.message());
        cb(ec);
        return;
      }

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
};

int main() {
  asio::io_context ctx;

  StreamTransmitApp app{ctx};
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
