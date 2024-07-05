#include "decoder.hpp"

#include "log.hpp"

#include <edge264.h>

class DecoderImpl : public Decoder {
 public:
  bool initialize() {
    m_stream = Edge264_alloc();
    if (!m_stream) {
      LOG_ERROR("Failed allocating edge264 stream");
      return false;
    }
    return true;
  }

  ~DecoderImpl() override {
    if (m_stream) {
      Edge264_free(&m_stream);
    }
  }

  virtual void decode_packet(VideoPacket p) override {
    // TODO: check for minimum number of bytes!

    const unsigned char* data = p.nal_data.data();
    const size_t size = p.nal_data.size();
    m_stream->CPB = data;
    m_stream->end = data + size;

    int res = Edge264_decode_NAL(m_stream);
    LOG_DEBUG("decode NAL returned: {}", res);
    res = Edge264_get_frame(m_stream, 0);
    LOG_DEBUG("Edge264_get_frame returned: {}", res);
  }

 private:
  Edge264_stream* m_stream{};
};

std::unique_ptr<Decoder> make_decoder() {
  auto instance = std::make_unique<DecoderImpl>();
  if (!instance->initialize()) {
    LOG_ERROR("Failed initializing decoder");
    return nullptr;
  }
  return instance;
}
