#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <vector>

#include "types.hpp"

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

class VideoCapture {
 public:
  virtual ~VideoCapture() = default;

  virtual void print_capabilities() = 0;
  virtual std::vector<std::unique_ptr<AbstractVideoFormatSpec>>
  enumerate_formats() = 0;
  virtual bool select_format(const AbstractVideoFormatSpec&) = 0;
  virtual void start() = 0;
  virtual void stop() = 0;
};

std::vector<std::filesystem::path> enumerate_video4_linux_devices();

std::unique_ptr<VideoCapture> make_video_capture(
    std::filesystem::path p,
    std::function<void(BufferView)> on_frame);
