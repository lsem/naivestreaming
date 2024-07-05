#pragma once

#include <memory>
#include "types.hpp"

class Decoder {
 public:
  virtual ~Decoder() = default;
  virtual void decode_packet(VideoPacket) = 0;
};

std::unique_ptr<Decoder> make_decoder();
