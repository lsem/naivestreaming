#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <system_error>

// Metadata of the frame coming from video capture.
struct CapturedFrameMeta {
  std::chrono::steady_clock::time_point timestamp;
};

// Definition taken from x264 header, can be seen as abstraction for all
// mpeg-like codec where NAL is common representation of encoded codec output.
enum class NAL_Type : uint8_t {
  __begin,
  unknown = __begin,
  slice = 1,
  slice_dpa = 2,
  slice_dpb = 3,
  slice_dpc = 4,
  slice_idr = 5,
  sei = 6,
  sps = 7,
  pps = 8,
  aud = 9,
  filler = 12,
  __end
};

std::string to_string(NAL_Type v);
std::ostream& operator<<(std::ostream& os, NAL_Type v);

// Encoder may produce some frame metadata related to both particular NAL or a
// frame.
struct NAL_Metadata {
  uint32_t timestamp;
  NAL_Type nal_type = NAL_Type::unknown;
  // If NAL caries slice, macroblock range (first, last) will indicate what
  // macroblocks are caried by this NAL. Supposed to be used by receiver to sort
  // NALs before feeding to decoder.
  int first_macroblock{};
  int last_macroblock{};
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
  NAL_Metadata nal_meta;
};
