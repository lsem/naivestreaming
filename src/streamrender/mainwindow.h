#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  MainWindow(int width,
             int height,
             std::string pixelformat,
             std::string fifo_path,
             QWidget* parent = nullptr);
  ~MainWindow();

  bool initialize();
  void start();
  void stop();

  // TODO: calc based on pixel format as well.
  size_t get_framebuff_size() const { return m_width * m_height * 3; }

  void closeEvent(QCloseEvent* bar) override { stop(); }

 private:
  Ui::MainWindow* ui;
  std::jthread m_stdin_read_th;
  int m_width{};
  int m_height{};
  std::string m_pixformat;
  std::string m_fifo_path;
  std::vector<char> m_readbuff;
  size_t m_readbuff_size{};
  int m_fifo_fd{};
};
#endif  // MAINWINDOW_H
