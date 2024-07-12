#pragma once

#include <memory>
#include "types.hpp"

class DecoderListener {
 public:
  virtual ~DecoderListener() = default;
  virtual void on_frame(VideoFrame f) = 0;
};

class Decoder {
 public:
  virtual ~Decoder() = default;
  virtual void decode_packet(VideoPacket) = 0;
};

std::unique_ptr<Decoder> make_decoder(DecoderListener& listener);
