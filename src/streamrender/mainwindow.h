#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <asio/io_context.hpp>
#include <cstdio>
#include <decoder.hpp>
#include <mutex>
#include <string>
#include <thread>
#include <udp_receive.hpp>
#include <vector>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow,
                   public UDP_ReceiveListener,
                   DecoderListener {
  Q_OBJECT

 public:
  MainWindow(asio::io_context& ctx,
             int width,
             int height,
             std::string pixelformat,
             QWidget* parent = nullptr);
  ~MainWindow();

  bool initialize();
  void start();
  void stop();

 public:  // UDP_ReceiveListener
  virtual void on_packet_received(VideoPacket p) override;

 public:  // DecoderListener
  virtual void on_frame(VideoFrame f) override;

 public:  // QWindow
  void paintEvent(QPaintEvent* event) override;

 public:
  // TODO: calc based on ppixel format as well.
  size_t get_framebuff_size() const { return m_width * m_height * 3; }
  void closeEvent(QCloseEvent* bar) override { stop(); }

 private:
  Ui::MainWindow* ui{};
  int m_width{};
  int m_height{};
  std::string m_pixformat;
  size_t m_readbuff_size{};
  std::unique_ptr<Decoder> m_decoder;
  std::unique_ptr<UDP_Receive> m_udp_receive;
  asio::io_context& m_ctx;
  int m_packets_received{};
  std::optional<VideoFrame> m_current_frame;
  std::mutex m_current_frame_lock;
};
#endif  // MAINWINDOW_H
