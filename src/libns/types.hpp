#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <system_error>

// Metadata of the frame coming from video capture.
struct CapturedFrameMeta {
  std::chrono::steady_clock::time_point timestamp;
};

// Encoder may produce some frame metadata related to both particular NAL or a
// frame.
struct EncodedFrameMetadata {
  std::chrono::steady_clock::time_point timestamp;
  uint32_t sequence_num{};
};

enum class PixelFormat { YUV422_packed, YUV422_planar };

// Represents non-working video frame.
struct VideoFrame {
  PixelFormat pixel_format;
  int width{};
  int height{};
  std::array<const uint8_t*, 3> planes;
};

template <class T>
struct CallbackTemplate {
  using type = std::function<void(std::error_code, T)>;
};

template <>
struct CallbackTemplate<void> {
  using type = std::function<void(std::error_code)>;
};

template <class T>
using callback = CallbackTemplate<T>::type;

// TODO: consider allocating all video packets in some fixed pool instead of
// using general allocator.
struct VideoPacket {
  std::vector<uint8_t> nal_data;
  uint32_t sequence_num{};
  std::chrono::steady_clock::time_point timestamp;
};
