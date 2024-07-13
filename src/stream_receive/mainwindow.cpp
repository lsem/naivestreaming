#include "mainwindow.h"
#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <QPainter>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <log.hpp>
#include "./ui_mainwindow.h"

LOG_MODULE_NAME("RCV_APP")

bool MainWindow::initialize() {
  m_decoder = make_decoder(*this);
  if (!m_decoder) {
    LOG_ERROR("failed creating decoder");
    return false;
  }

  m_udp_receive = make_udp_receive(m_ctx, 34000);
  if (!m_udp_receive) {
    LOG_ERROR("failed creating udp receive");
    return false;
  }

  return true;
}

void MainWindow::start() {
  m_udp_receive->start(*this);
}

void MainWindow::stop() {}

void MainWindow::on_packet_received(VideoPacket p) /*override*/ {
  m_packets_received++;
  m_decoder->decode_packet(std::move(p));
  update();
}

void MainWindow::on_frame(const VideoFrame& f) /*override*/ {
  LOG_DEBUG("Got a frame");

  assert(f.planes[0] != nullptr);
  assert(f.planes[1] != nullptr);
  assert(f.planes[2] != nullptr);

  // We are going to convert a frame to ready to display image. This transcoding
  // to RGB is not what we would do for real-work production app but is enough
  // for the purpose of displaying videostream.
  // In order to create QImage we are going to make use of one of constructors
  // like QImage(const uchar *data, int width, int height, ...) which allow to
  // pass data directly without calling setPixel width*height times. The target
  // format is going to be ARGB32 (0xAARRGGBB).
  // 422 planar description can be found in:
  // https://www.kernel.org/doc/html/v4.10/media/uapi/v4l/pixfmt-yuv422m.html
  std::unique_ptr<uchar[]> image_buffer{new uchar[f.width * f.height * 4]};

  const uint8_t* Y_plane = f.planes[0];
  const uint8_t* U_plane = f.planes[1];
  const uint8_t* V_plane = f.planes[2];

  auto yuv_to_rgb = [](uint8_t Y, uint8_t U, uint8_t V, uint8_t& R, uint8_t& G,
                       uint8_t& B) {
    const auto Rd = Y + 1.13983 * (V - 128);
    const auto Gd = Y - 0.39465 * (U - 128) - 0.58060 * (V - 128);
    const auto Bd = Y + 2.03211 * (U - 128);
    R = static_cast<uint8_t>(round(Rd));
    G = static_cast<uint8_t>(round(Gd));
    B = static_cast<uint8_t>(round(Bd));
  };

  assert(f.pixel_format == PixelFormat::YUV422_planar);

  for (size_t x = 0; x < f.width; ++x) {
    for (size_t y = 0; y < f.height; ++y) {
      const size_t offset = y * f.width + x;

      const auto Y = Y_plane[offset];
      const auto U = U_plane[offset / 2];
      const auto V = V_plane[offset / 2];

      uint8_t R, G, B;
      yuv_to_rgb(Y, U, V, R, G, B);

      image_buffer[offset * 4 + 3] = 255;
      image_buffer[offset * 4 + 2] = R;
      image_buffer[offset * 4 + 1] = G;
      image_buffer[offset * 4 + 0] = B;
    }
  }

  std::lock_guard locked{m_current_frame_lock};
  m_current_frame_data = std::move(image_buffer);
  m_current_frame_img =
      QImage{static_cast<const uchar*>(m_current_frame_data.get()), f.width,
             f.height, QImage::Format_ARGB32};

  update();
}

void MainWindow::paintEvent(QPaintEvent* event) /*override*/ {
  QPainter painter;
  painter.begin(this);

  // TODO: consider optimizting this by unlockign after fetch with data copying.
  // The current approach locks decoder in paintEvent.
  std::scoped_lock slock{m_current_frame_lock};
  painter.drawImage(rect(), m_current_frame_img);

  // srand(42);
  // QBrush brush{QColor{135, 135, 135, 100}};
  // for (int i = 0; i < m_packets_received; ++i) {
  //   painter.fillRect(QRectF(rand() % width(), rand() % height(), 5, 5), brush);
  // }

  painter.end();
}

MainWindow::MainWindow(asio::io_context& ctx,
                       int width,
                       int height,
                       std::string pixelformat,
                       QWidget* parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), m_ctx{ctx} {
  m_width = width;
  m_height = height;
  m_pixformat = pixelformat;
  ui->setupUi(this);
}

MainWindow::~MainWindow() {
  delete ui;
}
