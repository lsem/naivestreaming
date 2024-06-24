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

// VideoCapture specific depdendencies:
#include <linux/videodev2.h>

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
class VideoCaptureImpl : public VideoCapture {
 public:
  explicit VideoCaptureImpl(std::filesystem::path video_dev_fpath)
      : m_video_dev_fpath(std::move(video_dev_fpath)) {}

  bool initialize() {
    // What if we enumrate video devices first and let user select the device.
    m_v4l_fd = open(m_video_dev_fpath.c_str(), O_RDWR);
    if (m_v4l_fd == -1) {
      cerr << "ERROR: failed opening /dev/video0: " << strerror(errno) << "\n";
      return false;
    }
    return true;
  }

 public:
  virtual ~VideoCaptureImpl() override {
    if (close(m_v4l_fd) == -1) {
      cerr << "ERROR: failed closing v4l descriptor. Ignoring..\n";
    } else {
      cout << "DEBUG: v4l closed, capture destructed\n";
    }
  }

  virtual void print_capabilities() override {
    struct v4l2_capability caps = {0};

    int ret = ioctl(m_v4l_fd, VIDIOC_QUERYCAP, &caps);
    if (ret == -1) {
      perror("Querying device capabilities");
      return;
    }
    cout << "DEBUG: Driver: " << caps.driver << "\n";
    cout << "DEBUG: Card: " << caps.card << "\n";
    cout << "DEBUG: Bus Info: " << caps.bus_info << "\n";
    cout << "DEBUG: Capabilities: " << caps.capabilities << "\n";
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
      cerr << "ERROR: passed wrong video format, unrelated type\n";
      return false;
    }

    // TODO: why this must be hardcoded? I guess it can be part of public part
    // of FormatSpec.
    fmt.fmt.pix.width = f.basic.width;
    fmt.fmt.pix.height = f.basic.height;
    fmt.fmt.pix.pixelformat = p->pixel_format;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (ioctl(m_v4l_fd, VIDIOC_S_FMT, &fmt) == -1) {
      cerr << "ERROR: Could not set format description: " << strerror(errno)
           << "\n";
      return false;
    }

    cout << "DEBUG: format selected\n";

    return true;
  }

  virtual void start() override {
    // we need to set video format but first we need to get it.
    // In real system, this is something that stuff will hard code, because he
    // pr she knows what is available and what is needed, so here should also be
    // a method for querying possibilities.
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
    cerr << "ERROR: failed initializing video capture\n";
    return nullptr;
  }
  return impl;
}

int main() {
  auto devs = VideoCaptureImpl::enumerate_video4_linux_devices();

  cout << "DEBUG: Video4Linux devices:\n";
  for (auto& x : devs) {
    cout << x << "\n";
  }

  cout << "DEBUG: selecting first device in a list: " << devs.front() << "\n";
  auto capture = make_video_capture(devs.front());
  if (!capture) {
    cout << "ERROR: failed creating videocapture. Exiting..\n";
    return -1;
  }

  cout << "DEBUG: videocapture created\n";

  // Use cases:
  // how to select device? what if I have multiple devices in the system?

  capture->print_capabilities();
  auto formats = capture->enumerate_formats();
  if (formats.empty()) {
    cerr << "ERROR: no available video formats\n";
    return -1;
  }
  capture->select_format(*formats.back());

  capture->start();
}
