#pragma once

#include <memory>
#include "types.hpp"

class DecoderListener {
 public:
  virtual ~DecoderListener() = default;

  // The frame data is non-owning, i.e. is valid only for a time of call.
  virtual void on_frame(const VideoFrame& f) = 0;
};

class Decoder {
 public:
  virtual ~Decoder() = default;
  virtual void decode_packet(VideoPacket) = 0;
};

std::unique_ptr<Decoder> make_decoder(DecoderListener& listener);
