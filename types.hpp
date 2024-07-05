#pragma once

#include <functional>
#include <system_error>
#include <cstdint>

struct BufferView {
  void* start;
  size_t length;
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
};
