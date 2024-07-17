#include "encoder.hpp"
#include "log.hpp"

#include <x264.h>
#include <cassert>
#include <mutex>
#include <vector>

LOG_MODULE_NAME("ENCODER");

struct EncoderImpl;
namespace {
struct FrameUserData {
  EncoderImpl* this_{};
  CapturedFrameMeta captured_fmeta;
};
}  // namespace

class EncoderImpl : public Encoder {
 public:
  EncoderImpl(EncoderClient& client) : m_client(client) {}
  ~EncoderImpl() override {
    if (m_h) {
      LOG_DEBUG("Closing encoder");
      x264_encoder_close(m_h);
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

    if (x264_param_default_preset(&param, "faster", "zerolatency") < 0) {
      LOG_ERROR("Failed applying profile (first) ");
      return false;
    }

    // on my laptop the format of RAW is camera YUYV 4:2:2.
    // TODO: write translation from v4l to internal type and then internal type
    // to x264 format.
    LOG_WARNING("encoder input format is hardcoded");
    LOG_WARNING("dimensions hardcoded");

    // QUESTIONS:
    //    without max-NAL settings we will need to split NALS into 1500b pieces.
    //    The problem with this is that when we loose one fragment we loose
    //    entire NAL. Better would be to have frame split into some regions so
    //    different regions are encoded into different NALs.

    // #ifdef USE_BASELINE_PROFILE

    param.i_csp = X264_CSP_YUYV;  // yuyv 4:2:2 packed
    // TODO: take it from settings.
    param.i_width = 1280;
    param.i_height = 720;
    param.i_fps_num = 25;
    param.i_fps_den = 1;
    // 3    param.b_vfr_input = 1;
    param.b_intra_refresh = 1;
    param.b_repeat_headers = 1;
    param.b_annexb = 1;
    param.i_frame_total = 0;

    //    param.i_keyint_max = 25;

    // param.rc.i_rc_method = X264_RC_CRF;
    // param.rc.f_rf_constant = 22;
    // param.rc.i_vbv_max_bitrate = 2000;
    // param.rc.i_vbv_buffer_size = 2000 / 30;

    param.nalu_process = [](x264_t* h, x264_nal_t* nal, void* opaque) {
      // WARNING: This is going to be called from internal thread of x264,
      // depending on multu-threading settings.
      // In order to make client life simpler and not burden it with
      // additional locking lets provide serialized callback notifications
      // as guarantee and see if it works. It should, because I expect that
      // application will just copy NAL into a buffer and pass it into some
      // sort of output queue or even into the socket buffer. Anyways, this
      // should be profiled before planning any optimization.

      LOG_DEBUG("Produced NAL of type: {}", nal->i_type);

      auto& user_data = *static_cast<FrameUserData*>(opaque);
      auto this_ = user_data.this_;

      assert((nal->i_payload * 3) / 2 + 5 + 64 <
             this_->m_nal_encoding_buff.size());

      x264_nal_encode(h, this_->m_nal_encoding_buff.data(), nal);

      LOG_DEBUG("sending NAL of {}, first MB: {}, last MB: {}", nal->i_payload,
                nal->i_first_mb, nal->i_last_mb);

      std::lock_guard lck{this_->m_client_notification_lock};
      // TODO: where do I get frame data?
      this_->m_client.on_nal_encoded(
          std::span{nal->p_payload, nal->p_payload + nal->i_payload},
          EncodedFrameMetadata{.timestamp = user_data.captured_fmeta.timestamp,
                               .sequence_num = 0});
    };

    // TODO: calculate this value correctly.
    m_nal_encoding_buff.resize(1920 * 1080 * 10);

    //    if (x264_param_apply_profile(&param, "high422") < 0) {
    if (x264_param_apply_profile(&param, "high422") < 0) {
      LOG_ERROR("Failed applying profile (second)");
      return false;
    }

    LOG_DEBUG("Profile applied");

    // lsem: as far as I understand, there are two ways how we can transmit
    // slices via IP networks: 1) splitting NALs that don't fit into a one
    // packet on protocol level. 2) using slicing and limitting maximum slice
    // size to MTU size of the network. (2) allows seems to be more versatilize
    // allowing to not retransmit occasional lost packets.
    //    The client can decide if it needs to ask for retransmition or just
    //    display someting else instead of missed slice.
    param.i_slice_max_size = 1400;
    param.i_threads = 1;
    param.b_sliced_threads = 0;

    auto picture = std::make_unique<x264_picture_t>();

    x264_picture_init(picture.get());
    // x264_picture_alloc(picture.get(), param.i_csp, param.i_width,
    //                    param.i_height);
    picture->img.i_csp = param.i_csp;
    picture->img.i_plane = 1;

    m_h = x264_encoder_open(&param);
    if (!m_h) {
      LOG_ERROR("Failed opening encoder");
      return false;
    }

    m_pic = std::move(picture);

    // LSEM: with current settings we can have as many as 70 delayed frames
    // before we start getting frames. How we are supposed to start streaming
    // with this?
    auto max_delayed_frames = x264_encoder_maximum_delayed_frames(m_h);
    LOG_DEBUG("max_delayed_frames: {}", max_delayed_frames);

    LOG_DEBUG("Encoder settings:");
    LOG_DEBUG("Threads: {}", param.i_threads);
    LOG_DEBUG("Sliced Threads: {}", param.b_sliced_threads);
    LOG_DEBUG("FPS: {}", param.i_fps_num);

    assert(param.i_threads == 1);
    assert(param.b_sliced_threads == 0);
    assert(param.b_intra_refresh);

    return true;
  }

  virtual void process_frame(std::span<uint8_t> data,
                             CapturedFrameMeta meta) override {
    m_client.on_frame_started();

    x264_picture_t pic_out{};
    x264_nal_t* nal{};
    int i_nal{};

    m_pic->img.i_plane = 2;
    assert(m_pic->img.i_plane == 2);

    const size_t width = 1280;
    const size_t height = 720;

    // Packed YUYV 422 has only one plain.
    m_pic->img.plane[0] = data.data();
    m_pic->img.i_stride[0] = 1280 * 2;

    // TODO: what is PTS?
    m_pic->i_pts = m_frame;
    LOG_DEBUG("frame: {}", m_pic->i_pts);

    // Allcate user data on stack of this thread,  the pointer will be valid
    // for as long as x264_encoder_encode is working so we can avoid heap
    // memory allocation.
    FrameUserData user_data;

    m_pic->opaque = &user_data;

    LOG_DEBUG("Start encode");
    int frame_size =
        x264_encoder_encode(m_h, &nal, &i_nal, m_pic.get(), &pic_out);
    if (frame_size < 0) {
      LOG_ERROR("Failed encoding frame {}", m_frame);
      // TODO: consider not to fail immidiately.
      return;
    } else if (frame_size) {
      m_client.on_frame_ended();

      LOG_DEBUG(
          "Encoded frame {} (nals count: {}, nal payload size: {}, frame "
          "size: "
          "{})",
          m_frame, i_nal, nal->i_payload, frame_size);
    }

    m_frame++;
  }

 private:
  EncoderClient& m_client;
  x264_t* m_h{};
  std::unique_ptr<x264_picture_t> m_pic{};
  int m_frame{};
  std::mutex m_client_notification_lock;
  std::vector<uint8_t> m_nal_encoding_buff;
};

std::unique_ptr<Encoder> make_encoder(EncoderClient& client) {
  auto instance = std::make_unique<EncoderImpl>(client);
  if (!instance->initialize()) {
    LOG_ERROR("Failed initializing encoder");
    return nullptr;
  }
  return instance;
}
