#pragma once

#include <memory>
#include <span>
#include "types.hpp"

class EncoderClient {
 public:
  virtual ~EncoderClient() = default;

  virtual void on_frame_started() = 0;
  virtual void on_frame_ended() = 0;
  virtual void on_nal_encoded(std::span<const uint8_t> data,
                              NAL_Metadata meta) = 0;
};

class Encoder {
 public:
  virtual ~Encoder() = default;

  // NOTE: because x264 requires is to pass non-const piece of data we define
  // this interface like this.
  virtual void process_frame(std::span<uint8_t> data,
                             CapturedFrameMeta meta) = 0;
};

std::unique_ptr<Encoder> make_encoder(EncoderClient& client);
