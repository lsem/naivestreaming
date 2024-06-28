#pragma once

#include <memory>
#include "types.hpp"

class Encoder {
 public:
  virtual ~Encoder() = default;
  virtual void process_frame(BufferView& buff) = 0;
};

std::unique_ptr<Encoder> make_encoder();
