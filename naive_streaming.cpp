#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
#include <vector>

#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <asio/io_context.hpp>
#include <asio/signal_set.hpp>

// VideoCapture specific depdendencies:
#include <linux/videodev2.h>

#include <format>
#include <string_view>

enum class LogLevel { debug, info, warning, error };

namespace lsem_log_details {
template <class... Args>
void print_log(LogLevel level, std::string_view fmt, Args&&... args) {
  auto label_fn = [](LogLevel level) {
    switch (level) {
      case LogLevel::debug:
        return "DEBUG";
      case LogLevel::info:
        return "INFO";
      case LogLevel::warning:
        return "WARNING";
      case LogLevel::error:
        return "ERROR";
      default:
        return "LogLevel::<unknown>";
    }
  };
  std::cout << label_fn(level) << ": "
            << std::vformat(fmt, std::make_format_args(args...)) << "\n";
}
}  // namespace lsem_log_details
#define LOG_DEBUG(FmtMsg, ...) \
  lsem_log_details::print_log(LogLevel::debug, FmtMsg, ##__VA_ARGS__)
#define LOG_INFO(FmtMsg, ...) \
  lsem_log_details::print_log(LogLevel::info, FmtMsg, ##__VA_ARGS__)
#define LOG_WARNING(FmtMsg, ...) \
  lsem_log_details::print_log(LogLevel::warning, FmtMsg, ##__VA_ARGS__)
#define LOG_ERROR(FmtMsg, ...) \
  lsem_log_details::print_log(LogLevel::error, FmtMsg, ##__VA_ARGS__)

using namespace std;

/* **SUBTASKS**
 *   1) component responsible for capturing video (getting a constant flow of
 * image frames) 2) component for encoding captrured video stream. 3) Component
 * for sending video stream. 4) Component for receiving video stream. 5)
 * Component for decoding video stream. 6) Component for displaying video
 * stream.
 */

// This video format spec in fact should be retrieved from enumeration phase.
// Do we even need an abstraction here? I guess so.
// In fact what I want to do is to have type erasad polymorphic value that can
// be casted back by implementation in dynamic_cast fashion. The problem is
// that it is rather hard to implement type erasure. So as a prototype we
// implement it in classic OOP fashion instead.
struct AbstractVideoFormatSpec {
  virtual ~AbstractVideoFormatSpec() = default;
  struct Basic {
    uint32_t width{};
    uint32_t height{};
  } basic;
  AbstractVideoFormatSpec(Basic basic) : basic(basic) {}
};

struct Video4LinuxVideoFormat : public AbstractVideoFormatSpec {
  explicit Video4LinuxVideoFormat(AbstractVideoFormatSpec::Basic basic,
                                  uint32_t pixel_format)
      : AbstractVideoFormatSpec(basic), pixel_format(pixel_format) {}
  // Here we may have all v4l private stuff.
  uint32_t pixel_format{};
};

class VideoCapture {
 public:
  virtual ~VideoCapture() = default;

  virtual void print_capabilities() = 0;
  virtual vector<std::unique_ptr<AbstractVideoFormatSpec>>
  enumerate_formats() = 0;
  virtual bool select_format(const AbstractVideoFormatSpec&) = 0;
  virtual void start() = 0;
  virtual void stop() = 0;
};

// Implenebtation based on Video4Linux.
// Documentation:
//  1) https://docs.kernel.org/4.20/media/v4l-drivers/index.html
//  2) https://github.com/kmdouglass/v4l2-examples
//  3)
//  https://stackoverflow.com/questions/10634537/v4l2-difference-between-enque-deque-and-queueing-of-the-buffer
class VideoCaptureImpl : public VideoCapture {
 public:
  explicit VideoCaptureImpl(std::filesystem::path video_dev_fpath)
      : m_video_dev_fpath(std::move(video_dev_fpath)) {}

  bool initialize() {
    // What if we enumrate video devices first and let user select the device.
    m_v4l_fd = open(m_video_dev_fpath.c_str(), O_RDWR);
    if (m_v4l_fd == -1) {
      LOG_ERROR("failed opening /dev/video0: {}", strerror(errno));
      return false;
    }
    return true;
  }

 public:
  virtual ~VideoCaptureImpl() override {
    if (close(m_v4l_fd) == -1) {
      LOG_ERROR("ERROR: failed closing v4l descriptor. Ignoring..");
    } else {
      LOG_ERROR("closed, capture destructed");
    }
  }

  virtual void print_capabilities() override {
    struct v4l2_capability caps = {0};

    int ret = ioctl(m_v4l_fd, VIDIOC_QUERYCAP, &caps);
    if (ret == -1) {
      perror("Querying device capabilities");
      return;
    }
    auto as_cstr = [](auto barr) {
      return reinterpret_cast<const char*>(&barr[0]);
    };
    LOG_DEBUG(
        "Capabilities:\n\tDriver: {}\n\tCard: {}\n\tBus Info: "
        "{}\nCapabilities:",
        as_cstr(caps.driver), as_cstr(caps.card), as_cstr(caps.bus_info));

#define PROCESS_CAP(C)         \
  if (caps.capabilities & C) { \
    LOG_DEBUG("{}", #C);       \
  }                            \
  while (false)
    PROCESS_CAP(V4L2_CAP_VIDEO_CAPTURE);
    PROCESS_CAP(V4L2_CAP_VIDEO_OUTPUT);
    PROCESS_CAP(V4L2_CAP_VIDEO_OVERLAY);
    PROCESS_CAP(V4L2_CAP_VBI_CAPTURE);
    PROCESS_CAP(V4L2_CAP_VBI_OUTPUT);
    PROCESS_CAP(V4L2_CAP_SLICED_VBI_CAPTURE);
    PROCESS_CAP(V4L2_CAP_SLICED_VBI_OUTPUT);
    PROCESS_CAP(V4L2_CAP_RDS_CAPTURE);
    PROCESS_CAP(V4L2_CAP_VIDEO_OUTPUT_OVERLAY);
    PROCESS_CAP(V4L2_CAP_HW_FREQ_SEEK);
    PROCESS_CAP(V4L2_CAP_RDS_OUTPUT);
    PROCESS_CAP(V4L2_CAP_VIDEO_CAPTURE_MPLANE);
    PROCESS_CAP(V4L2_CAP_VIDEO_OUTPUT_MPLANE);
    PROCESS_CAP(V4L2_CAP_VIDEO_M2M_MPLANE);
    PROCESS_CAP(V4L2_CAP_VIDEO_M2M);
    PROCESS_CAP(V4L2_CAP_TUNER);
    PROCESS_CAP(V4L2_CAP_AUDIO);
    PROCESS_CAP(V4L2_CAP_RADIO);
    PROCESS_CAP(V4L2_CAP_MODULATOR);
    PROCESS_CAP(V4L2_CAP_SDR_CAPTURE);
    PROCESS_CAP(V4L2_CAP_EXT_PIX_FORMAT);
    PROCESS_CAP(V4L2_CAP_SDR_OUTPUT);
    PROCESS_CAP(V4L2_CAP_META_CAPTURE);
    PROCESS_CAP(V4L2_CAP_READWRITE);
    PROCESS_CAP(V4L2_CAP_ASYNCIO);
    PROCESS_CAP(V4L2_CAP_STREAMING);
    PROCESS_CAP(V4L2_CAP_META_OUTPUT);
    PROCESS_CAP(V4L2_CAP_TOUCH);
    PROCESS_CAP(V4L2_CAP_IO_MC);
    PROCESS_CAP(V4L2_CAP_DEVICE_CAPS);
#undef PROCESS_CAP
  }

  virtual vector<std::unique_ptr<AbstractVideoFormatSpec>> enumerate_formats()
      override {
    vector<std::unique_ptr<AbstractVideoFormatSpec>> result;
    struct v4l2_fmtdesc fmtdesc = {0};
    fmtdesc.index = 0;
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    while (ioctl(m_v4l_fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
      const char c = fmtdesc.flags & 1 ? 'C' : ' ';
      const char e = fmtdesc.flags & 2 ? 'E' : ' ';

      printf("%c%c %s\n", c, e, fmtdesc.description);
      fmtdesc.index++;

      result.emplace_back(std::make_unique<Video4LinuxVideoFormat>(
          Video4LinuxVideoFormat::Basic{.width = 320, .height = 200},
          fmtdesc.pixelformat));
    }

    return result;
  }

  virtual bool select_format(const AbstractVideoFormatSpec& f) override {
    // Set the device format
    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    // TODO: once we implement real type erasure, we will not need dynamic cast
    // and RTTI dependency anymore.
    auto* p = dynamic_cast<const Video4LinuxVideoFormat*>(&f);
    if (!p) {
      LOG_ERROR("passed wrong video format, unrelated type: {}",
                strerror(errno));
      return false;
    }

    // TODO: why this must be hardcoded? I guess it can be part of public part
    // of FormatSpec.
    fmt.fmt.pix.width = f.basic.width;
    fmt.fmt.pix.height = f.basic.height;
    fmt.fmt.pix.pixelformat = p->pixel_format;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (ioctl(m_v4l_fd, VIDIOC_S_FMT, &fmt) == -1) {
      LOG_ERROR("Could not set format description: {}", strerror(errno));
      return false;
    }

    LOG_DEBUG("format selected");

    return true;
  }

  virtual void start() override {
    // we need to set video format but first we need to get it.
    // In real system, this is something that stuff will hard code, because he
    // pr she knows what is available and what is needed, so here should also be
    // a method for querying possibilities.

    struct v4l2_requestbuffers reqbuf;
    struct {
      void* start;
      size_t length;
    } * buffers;
    unsigned int i;

    memset(&reqbuf, 0, sizeof(reqbuf));
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory = V4L2_MEMORY_MMAP;
    reqbuf.count = 20;

    // if (-1 == ioctl(fd, VIDIOC_REQBUFS, &reqbuf)) {
    //   if (errno == EINVAL)
    //     printf("Video capturing or mmap-streaming is not supported\\n");
    //   else
    //     perror("VIDIOC_REQBUFS");

    //   exit(EXIT_FAILURE);
    // }
  }

  virtual void stop() override {
    //..
  }

  // TODO: make it a free function.
  // https://www.linuxtv.org/wiki/index.php/Device_nodes_and_character_devices#V4L_character_devices
  static vector<std::filesystem::path> enumerate_video4_linux_devices() {
    vector<std::filesystem::path> result;
    for (auto& di : std::filesystem::directory_iterator{"/dev/"}) {
      const auto& fname = di.path().filename().string();
      if (di.is_character_file() && fname.size() == 6 &&
          fname.find("video") == 0) {
        result.emplace_back(di.path());
      }
    }
    std::sort(result.begin(), result.end());
    return result;
  }

 private:
  std::filesystem::path m_video_dev_fpath;
  int m_v4l_fd{-1};
};

std::unique_ptr<VideoCapture> make_video_capture(std::filesystem::path p) {
  auto impl = std::make_unique<VideoCaptureImpl>(std::move(p));
  if (!impl->initialize()) {
    LOG_ERROR("failed initializing video capture");
    return nullptr;
  }
  return impl;
}

int main() {
  auto devs = VideoCaptureImpl::enumerate_video4_linux_devices();
  LOG_DEBUG("Video4Linux devices:");
  for (auto& x : devs) {
    cout << x << "\n";
  }

  cout << "DEBUG: selecting first device in a list: " << devs.front() << "\n";
  auto capture = make_video_capture(devs.front());
  if (!capture) {
    LOG_ERROR("failed creating videocapture. Exiting..");
    return -1;
  }

  cout << "DEBUG: videocapture created\n";

  capture->print_capabilities();
  auto formats = capture->enumerate_formats();
  if (formats.empty()) {
    LOG_ERROR("no available video formats");
    return -1;
  }
  capture->select_format(*formats.back());

  // Starting capturing. Starting is resource heavy.
  capture->start();

  asio::io_context ctx;
  asio::signal_set signals{ctx, SIGINT, SIGTERM};
  signals.async_wait([&ctx](std::error_code ec, int signal) {
    LOG_DEBUG("closing app on request");
    ctx.stop();
  });

  ctx.run();
}