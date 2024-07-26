#include "decoder.hpp"

#include <signal.h>
#include <cassert>

#include "log.hpp"

LOG_MODULE_NAME("DECODER");

extern "C" {
#include <libavcodec/avcodec.h>
}

class DecoderImpl : public Decoder {
 public:
  explicit DecoderImpl(DecoderListener& listener) : m_listener(listener) {}

  bool initialize() {
    m_packet = av_packet_alloc();
    if (!m_packet) {
      LOG_ERROR("Failed allocating packet");
      return false;
    }

    m_codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!m_codec) {
      LOG_ERROR("h264 codec not found");
      return false;
    }

    m_parser_ctx = av_parser_init(m_codec->id);
    if (!m_parser_ctx) {
      LOG_ERROR("Parser not found");
      return false;
    }

    // TODO: add checks.
    m_codec_ctx = avcodec_alloc_context3(m_codec);
    if (!m_codec_ctx) {
      LOG_ERROR("Failed allocating codec context");
      return false;
    }

    // TODO: I can't see the effect of this. Check.
    // m_codec_ctx->flags2 |= AV_CODEC_FLAG2_CHUNKS;
    // m_codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    // Because we are going to draw our data on our own and we want to have only
    // one implementation we need to select format. Otherwise it will be
    // selected automatically as first non-hardware-accelerated-only format in
    // the enum (see
    // https://stackoverflow.com/questions/9652760/how-to-set-decode-pixel-format-in-libavcodec)
    m_codec_ctx->get_format =
        [](struct AVCodecContext* s,
           const enum AVPixelFormat* fmt) -> AVPixelFormat {
      for (auto f = fmt; f; f++) {
        if (*f == AV_PIX_FMT_YUV422P) {
          LOG_DEBUG("found format we need");
          return *f;
        }
      }
      return AV_PIX_FMT_NONE;
    };

    m_frame = av_frame_alloc();
    if (!m_frame) {
      LOG_ERROR("Failed allocaint frame");
      return false;
    }

    if (avcodec_open2(m_codec_ctx, m_codec, nullptr) < 0) {
      LOG_ERROR("Could not open codec");
      return false;
    }

    return true;
  }

  ~DecoderImpl() override {
    if (m_packet) {
      av_packet_free(&m_packet);
    }
  }

  bool decode_packet_impl(std::optional<VideoPacket> maybe_p) {
    // TODO: check for minimum number of bytes!

    auto& p = *maybe_p;
    LOG_DEBUG("Parsing packet of size {} bytes", p.nal_data.size());

    const size_t data_size = p.nal_data.size();
    p.nal_data.resize(p.nal_data.size() + AV_INPUT_BUFFER_PADDING_SIZE);
    const uint8_t* data = p.nal_data.data();

    int ret = av_parser_parse2(m_parser_ctx, m_codec_ctx, &m_packet->data,
                               &m_packet->size, data, data_size, AV_NOPTS_VALUE,
                               AV_NOPTS_VALUE, 0);
    LOG_DEBUG("ret: {}", ret);

    if (m_packet->size == 0) {
      LOG_DEBUG("Not a full packet yet, skipping");
      return false;
    }

    LOG_DEBUG("Reassempled full packet, the size is: {}", m_packet->size);

    ret = avcodec_send_packet(m_codec_ctx, m_packet);
    if (ret < 0) {
      LOG_ERROR("Failed sending packet for decoding: {}", ret);
      // TODO: needs to be reset?
      return false;
    }

    while (ret >= 0) {
      ret = avcodec_receive_frame(m_codec_ctx, m_frame);
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        LOG_DEBUG("Reached end of frames");
      } else if (ret < 0) {
        LOG_ERROR("Error during decoding: {}", ret);
        // TODO: fail decoder.
      } else {
        assert(m_frame->format == AV_PIX_FMT_YUV422P);
        assert(m_frame->data[0]);
        assert(m_frame->data[1]);
        assert(m_frame->data[2]);
        assert(!m_frame->data[3]);
        const auto* Y_plane = m_frame->data[0];
        const auto* U_plane = m_frame->data[1];
        const auto* V_plane = m_frame->data[2];

        // TODO: don't hardcode 1280x720
        VideoFrame frame{.pixel_format = PixelFormat::YUV422_planar,
                         .width = 1280,
                         .height = 720,
                         .planes = {Y_plane, U_plane, V_plane}};
        m_listener.on_frame(std::move(frame));
      }
    }

    return true;
  }

  virtual void decode_packet(VideoPacket p) override {
    decode_packet_impl(std::move(p));
  }

 private:
  DecoderListener& m_listener;
  const AVCodec* m_codec{};
  AVCodecParserContext* m_parser_ctx{};
  AVCodecContext* m_codec_ctx{};
  AVFrame* m_frame{};
  AVPacket* m_packet{};
};

std::unique_ptr<Decoder> make_decoder(DecoderListener& listener) {
  auto instance = std::make_unique<DecoderImpl>(listener);
  if (!instance->initialize()) {
    LOG_ERROR("Failed initializing decoder");
    return nullptr;
  }
  return instance;
}
