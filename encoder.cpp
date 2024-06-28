#include "encoder.hpp"
#include "log.hpp"

#include <x264.h>

class EncoderImpl : public Encoder {
 public:
  ~EncoderImpl() override {
    if (m_h) {
      LOG_DEBUG("Closing encoder");
      x264_encoder_close(m_h);
    }

    if (m_pic) {
      x264_picture_clean(m_pic.get());
    }
  }

  bool initialize() {
    LOG_DEBUG("initializing encoder");

    x264_param_t param{};

    x264_picture_t pic_out{};

    int i_frame{};
    int i_frame_size{};
    x264_nal_t* nal{};
    int i_nal{};

    if (x264_param_default_preset(&param, "medium", NULL) < 0) {
      LOG_ERROR("Failed applying profile (first) ");
      return false;
    }

    // on my laptop the format of RAW is camera YUYV 4:2:2.
    // TODO: write translation from v4l to internal type and then internal type
    // to x264 format.
    LOG_WARNING("encoder input format is hardcoded");
    LOG_WARNING("dimensions hardcoded");

    param.i_csp = X264_CSP_YUYV;  // yuyv 4:2:2 packed
    param.i_width = 320;
    param.i_height = 200;
    param.b_vfr_input = 1;
    param.b_repeat_headers = 1;
    param.b_annexb = 1;

    if (x264_param_apply_profile(&param, "high422") < 0) {
      LOG_ERROR("Failed applying profile (second)");
      return false;
    }

    LOG_DEBUG("Profile applied");

    auto picture = std::make_unique<x264_picture_t>();

    if (x264_picture_alloc(picture.get(), param.i_csp, param.i_width,
                           param.i_height) < 0) {
      LOG_ERROR("Failed allocating picture");
      return false;
    }

    m_h = x264_encoder_open(&param);
    if (!m_h) {
      LOG_ERROR("Failed opening encoder");
      return false;
    }

    m_pic = std::move(picture);

    return true;
  }

  virtual void process_frame(BufferView& buff) override {
    x264_picture_t pic_out{};
    x264_nal_t* nal{};
    int i_nal{};

    m_pic->img.plane[0] = reinterpret_cast<uint8_t*>(buff.start);

    // TODO: what is PTS?
    m_pic->i_pts = m_frame;

    int frame_size =
        x264_encoder_encode(m_h, &nal, &i_nal, m_pic.get(), &pic_out);
    if (frame_size < 0) {
      LOG_ERROR("Failed encoding frame {}", m_frame);
      // TODO: consider not to fail immidiately.
      return;
    } else if (frame_size) {
      LOG_DEBUG("Encoded frame {}", m_frame);
    }

    m_frame++;
  }

 private:
  x264_t* m_h{};
  std::unique_ptr<x264_picture_t> m_pic{};
  int m_frame{};
};

std::unique_ptr<Encoder> make_encoder() {
  auto instance = std::make_unique<EncoderImpl>();
  if (!instance->initialize()) {
    LOG_ERROR("Failed initializing encoder");
    return nullptr;
  }
  return instance;
}
