// VideoCapture specific depdendencies:
#include <linux/videodev2.h>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <optional>

#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <thread>

#include "log.hpp"
#include "video_capture.hpp"

LOG_MODULE_NAME("CAPTURE");

namespace {
struct BufferView {
  void* start{};
  size_t length{};
};
}  // namespace

constexpr int V4L_BUFFERS_COUNT = 5;

struct Video4LinuxVideoFormat : public AbstractVideoFormatSpec {
  explicit Video4LinuxVideoFormat(AbstractVideoFormatSpec::Basic basic,
                                  uint32_t pixel_format)
      : AbstractVideoFormatSpec(basic), pixel_format(pixel_format) {}
  // Here we may have all v4l private stuff.
  uint32_t pixel_format{};
};

namespace {
int xioctl(int fd, int request, void* arg) {
  int r;
  do
    r = ioctl(fd, request, arg);
  while (-1 == r && EINTR == errno);
  return r;
}

}  // namespace

// Implenebtation based on Video4Linux.
// Documentation:
//  1) https://docs.kernel.org/4.20/media/v4l-drivers/index.html
//  2) https://lwn.net/Articles/240667/
//  3) https://github.com/kmdouglass/v4l2-examples
//  4)
//  https://stackoverflow.com/questions/10634537/v4l2-difference-between-enque-deque-and-queueing-of-the-buffer
class VideoCaptureImpl : public VideoCapture {
 public:
  explicit VideoCaptureImpl(std::filesystem::path video_dev_fpath,
                            std::function<void(std::span<uint8_t>)> on_frame)
      : m_video_dev_fpath(std::move(video_dev_fpath)),
        m_on_frame(std::move(on_frame)) {}

  bool initialize() {
    // What if we enumrate video devices first and let user select the
    // device.
    m_v4l_fd = open(m_video_dev_fpath.c_str(), O_RDWR);
    if (m_v4l_fd == -1) {
      LOG_ERROR("failed opening /dev/video0: {}", strerror(errno));
      return false;
    }
    return true;
  }

 public:
  virtual ~VideoCaptureImpl() override { close_v4l_fd(); }

  void close_v4l_fd() {
    if (m_v4l_fd != -1) {
      if (close(m_v4l_fd) == -1) {
        LOG_ERROR("ERROR: failed closing v4l descriptor. Ignoring..");
      } else {
        LOG_DEBUG("closed, capture destructed");
        m_v4l_fd = -1;
      }
    }
  }

  virtual void print_capabilities() override {
    struct v4l2_capability caps = {0};

    int ret = ioctl(m_v4l_fd, VIDIOC_QUERYCAP, &caps);
    if (ret == -1) {
      LOG_ERROR("Querying device capabilities: {}", strerror(errno));
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
    LOG_DEBUG("> {}", #C);     \
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

    // TODO: check caps!
  }

  virtual std::vector<std::unique_ptr<AbstractVideoFormatSpec>>
  enumerate_formats() override {
    std::vector<std::unique_ptr<AbstractVideoFormatSpec>> result;
    struct v4l2_fmtdesc fmtdesc = {0};
    fmtdesc.index = 0;
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    while (ioctl(m_v4l_fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
      const char c = fmtdesc.flags & 1 ? 'C' : ' ';
      const char e = fmtdesc.flags & 2 ? 'E' : ' ';

      LOG_DEBUG("{}{} {}", c, e, (const char*)fmtdesc.description);
      fmtdesc.index++;

      result.emplace_back(std::make_unique<Video4LinuxVideoFormat>(
          Video4LinuxVideoFormat::Basic{.width = 1280, .height = 720},
          fmtdesc.pixelformat));
      // result.back()->basic.width = frmsize.discrete.width; //
      // result.back()->basic.height = frmsize.discrete.height;
      // result.back()->basic.width = 100;
      // result.back()->basic.height = 200;

      // Frame sizes
      struct v4l2_frmsizeenum frmsize {};
      frmsize.pixel_format = fmtdesc.pixelformat;
      frmsize.index = 0;
      std::string sep = "";
      std::string frame_sizes_s;
      while (ioctl(m_v4l_fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) >= 0) {
        if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
          frame_sizes_s += sep + std::format("{}x{}", frmsize.discrete.width,
                                             frmsize.discrete.height);
        } else if (frmsize.type == V4L2_FRMSIZE_TYPE_STEPWISE) {
          frame_sizes_s +=
              sep + std::format("{}x{} (stepwise)", frmsize.discrete.width,
                                frmsize.discrete.height);
        } else {
          LOG_WARNING("Other framesize type");
        }
        sep = ", ";
        frmsize.index++;

        // Frame intervals
        LOG_DEBUG("Frame intervals:");
        struct v4l2_frmivalenum frame_interval;
        while (ioctl(m_v4l_fd, VIDIOC_ENUM_FRAMEINTERVALS, &frame_interval) >=
               0) {
          LOG_DEBUG("Frame interval type: {}", frame_interval.type);
          if (frame_interval.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
            frame_interval.discrete;
          } else if (frame_interval.type == V4L2_FRMIVAL_TYPE_CONTINUOUS) {
            // ..
          } else if (frame_interval.type == V4L2_FRMIVAL_TYPE_STEPWISE) {
            auto& sw = frame_interval.stepwise;
            LOG_DEBUG("DISCRETE:  MIN: {}/{}, MAX: {}/{}, STEP: {}/{}",
                      sw.min.numerator, sw.min.denominator, sw.max.numerator,
                      sw.max.denominator, sw.step.numerator,
                      sw.step.denominator);
          }
        }
      }
      LOG_DEBUG("Frame sizes: {}", frame_sizes_s);
    }

    return result;
  }

  virtual bool select_format(const AbstractVideoFormatSpec& f) override {
    // Set the device format
    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    // TODO: once we implement real type erasure, we will not need dynamic
    // cast and RTTI dependency anymore.
    auto* p = dynamic_cast<const Video4LinuxVideoFormat*>(&f);
    if (!p) {
      LOG_ERROR("passed wrong video format, unrelated type: {}",
                strerror(errno));
      return false;
    }

    // TODO: why this must be hardcoded? I guess it can be part of public
    // part of FormatSpec.
    fmt.fmt.pix.width = f.basic.width;
    fmt.fmt.pix.height = f.basic.height;
    fmt.fmt.pix.pixelformat = p->pixel_format;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (ioctl(m_v4l_fd, VIDIOC_S_FMT, &fmt) == -1) {
      LOG_ERROR("Could not set format description: {}", strerror(errno));
      return false;
    }

    LOG_DEBUG("Format selected");

    return true;
  }

  void start_capture() {
    LOG_DEBUG("Starting capturing");

    struct v4l2_buffer buffer;
    for (size_t i = 0; i < m_allocated_buffers_count; ++i) {
      memset(&buffer, 0, sizeof(buffer));
      buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      buffer.memory = V4L2_MEMORY_MMAP;
      buffer.index = i;

      if (xioctl(m_v4l_fd, VIDIOC_QBUF, &buffer) == -1) {
        LOG_ERROR("IDIOC_QBUF failed for buff {}: {}", i, strerror(errno));
        // TODO: handle errors.
        return;
      }
      LOG_DEBUG("Enqued buffer {}", i);
    }
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(m_v4l_fd, VIDIOC_STREAMON, &type) == -1) {
      LOG_ERROR("VIDIOC_STREAMON failed: {}", strerror(errno));
      // TODO: handle errors.
      return;
    }

    LOG_DEBUG("Video capture streaming ON");
  }

  bool is_closing() const {
    return m_v4l_fd == -1;
  }
  // Returns true of frame is read successfully.
  // TODO: write a message explaining this.
  std::optional<bool> read_frame() {
    static int frame_num = 0;
    struct v4l2_buffer buff;
    memset(&buff, 0, sizeof(buff));

    buff.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buff.memory = V4L2_MEMORY_MMAP;

    // read_frame() expected to be called once select reported ready state
    // which means that there are buffers (at least one) ready with frame
    // data in outgoing queues of v4l2 driver. VIDIOC_DQBUF claims the
    // buffer out of v4l driver's queue.
    if (xioctl(m_v4l_fd, VIDIOC_DQBUF, &buff) == -1) {
      if (is_closing()) {
        return std::nullopt;
      }
      if (errno == EAGAIN) {
        return std::nullopt;
      } else {
        LOG_ERROR("VIDIOC_DQBUF failed: {}", strerror(errno));
        return std::nullopt;
      }
    }

    // NOTE:
    // Processing image means that we may want to do some automated post
    // processing of videostream in order to make things better visible.
    // Efficient architecture for that would be DMA from video camera to
    // something available for GPU processing. Withuot this, data transfer
    // to GPU and back will consume considerable amount of data bus
    // bandwidth. The focus of naivestreaming project is not processing so
    // we leave this question open to realworld applications where
    // corresponding hardware can be present.

    // So the next step is to feed this into x264.

    // LOG_DEBUG("Processing image from buffer ({}) ... (simulated) [{}]",
    //           buff.index, frame_num++);
    const auto buffer_data = static_cast<uint8_t*>(m_buffers[buff.index].start);
    const size_t buffer_data_size = m_buffers[buff.index].length;
    m_on_frame({buffer_data, buffer_data + buffer_data_size});

    // After processing we put the buffer back with VIDIOC_QBUF so it can be
    // used.
    if (xioctl(m_v4l_fd, VIDIOC_QBUF, &buff) == -1) {
      if (is_closing()) {
        return std::nullopt;
      }

      LOG_ERROR("VIDIOC_QBUF failed: {}", strerror(errno));
      // TODO: handle error.
      return std::nullopt;
    }

    return true;
  }

  virtual void start() override {
    assert(m_buffers.empty());

    // we need to set video format but first we need to get it.
    // In real system, this is something that stuff will hard code, because
    // he pr she knows what is available and what is needed, so here should
    // also be a method for querying possibilities.

    LOG_DEBUG("Initializing device buffers");

    struct v4l2_requestbuffers reqbuf;

    memset(&reqbuf, 0, sizeof(reqbuf));
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory = V4L2_MEMORY_MMAP;
    reqbuf.count = V4L_BUFFERS_COUNT;

    if (ioctl(m_v4l_fd, VIDIOC_REQBUFS, &reqbuf) == -1) {
      if (errno == EINVAL) {
        LOG_ERROR("Video capturing or mmap-streaming is not supported: {}",
                  strerror(errno));
      } else {
        // TODO: report error via error code.
        LOG_ERROR("VIDIOC_REQBUFS failed: {}", strerror(errno));
        return;
      }
    }

    if (reqbuf.count < 5) {
      LOG_ERROR("Failed allocating all requested buffers: {}", strerror(errno));
      // TODO: report error via error code.
      return;
    } else if (reqbuf.count < V4L_BUFFERS_COUNT) {
      LOG_WARNING("Not all buffers have been allocated");
    }

    m_buffers.resize(reqbuf.count);

    const size_t allocated_buffers_count = reqbuf.count;

    struct v4l2_buffer buffer;
    for (size_t i = 0; i < allocated_buffers_count; ++i) {
      memset(&buffer, 0, sizeof(buffer));
      buffer.type = reqbuf.type;
      buffer.memory = V4L2_MEMORY_MMAP;
      buffer.index = i;

      // Request buffer information
      if (ioctl(m_v4l_fd, VIDIOC_QUERYBUF, &buffer)) {
        LOG_ERROR("VIDIOC_QUERYBUF failed for {}: {}", i, strerror(errno));
        // TODO: report error.
        return;
      }

      m_buffers[i].length = buffer.length;
      m_buffers[i].start = mmap(NULL, buffer.length, PROT_READ | PROT_WRITE,
                                MAP_SHARED, m_v4l_fd, buffer.m.offset);
      if (m_buffers[i].start == MAP_FAILED) {
        LOG_ERROR("mmap of buffer {} failed: {}", i, strerror(errno));
        // TODO: report error.
        return;
      }

      LOG_DEBUG("Mapped buffer {}", i);
    }

    LOG_DEBUG("Device buffers initialized");

    m_allocated_buffers_count = allocated_buffers_count;

    start_capture();

    m_working_thread = std::jthread{[this](std::stop_token stoken) {
      // Reading loop.
      while (!stoken.stop_requested()) {
        fd_set fds;
        struct timeval tv;

        FD_ZERO(&fds);
        FD_SET(m_v4l_fd, &fds);
        tv.tv_sec = 2;
        tv.tv_usec = 0;

        int r = select(m_v4l_fd + 1, &fds, NULL, NULL, &tv);
        if (r == -1) {
          if (errno == EINTR) {
            continue;
          } else {
            LOG_ERROR("select failed: {}", r);
            // TODO: handle error.
            return;
          }
        } else if (r == 0) {
          LOG_ERROR("select timeout");
          // TODO: handle error.
          return;
        }

        if (auto maybe_res = read_frame(); maybe_res) {
          if (!*maybe_res) {
            // TODO: *************** ???????????? ******************
          }
        } else if (!is_closing()) {
          LOG_ERROR("read_frame failed");
          // TODO: handle error.
          return;
        }
      }
      LOG_DEBUG("Capture worker thread has stopped");
    }};
  }

  virtual void stop() override {
    if (m_working_thread.joinable()) {
      LOG_DEBUG("Requesting worker thread to stop");
      m_working_thread.request_stop();
      // Closing v4l to unblock select.
      close_v4l_fd();
      m_working_thread.join();
    }
  }

 private:
  std::filesystem::path m_video_dev_fpath;
  int m_v4l_fd{-1};
  unsigned m_allocated_buffers_count{};
  // Buffers we are sharing with v4l driver.
  std::vector<BufferView> m_buffers;
  std::function<void(std::span<uint8_t>)> m_on_frame;
  std::jthread m_working_thread;
};

// TODO: make it a free function.
// https://www.linuxtv.org/wiki/index.php/Device_nodes_and_character_devices#V4L_character_devices
std::vector<std::filesystem::path> enumerate_video4_linux_devices() {
  std::vector<std::filesystem::path> result;
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

std::unique_ptr<VideoCapture> make_video_capture(
    std::filesystem::path p,
    std::function<void(std::span<uint8_t>)> on_frame) {
  auto impl =
      std::make_unique<VideoCaptureImpl>(std::move(p), std::move(on_frame));
  if (!impl->initialize()) {
    LOG_ERROR("failed initializing video capture");
    return nullptr;
  }
  return impl;
}
