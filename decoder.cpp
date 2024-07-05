#include "decoder.hpp"

#include "log.hpp"

#include <edge264.h>

class DecoderImpl : public Decoder {
 public:
  bool initialize() {
    m_stream = Edge264_alloc();
    return true;
  }

  ~DecoderImpl() override {
    if (m_stream) {
      Edge264_free(&m_stream);
    }
  }

  virtual void decode_packet(VideoPacket p) override {
    LOG_ERROR("Not implemented");
  }

 private:
  Edge264_stream* m_stream{};
};

std::unique_ptr<Decoder> make_decoder() {
  auto instance = std::make_unique<DecoderImpl>();
  if (!instance) {
    LOG_ERROR("Failed initializing decoder");
    return nullptr;
  }
  return instance;
}
