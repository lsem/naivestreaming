#pragma once

#include <memory>
#include "types.hpp"

class EncoderClient {
 public:
  virtual ~EncoderClient() = default;

  virtual void on_frame_started() = 0;
  virtual void on_frame_ended() = 0;
  virtual void on_nal_encoded(const uint8_t* data, size_t data_size) = 0;
};

class Encoder {
 public:
  virtual ~Encoder() = default;
  virtual void process_frame(BufferView& buff) = 0;
};

std::unique_ptr<Encoder> make_encoder(EncoderClient& client);
