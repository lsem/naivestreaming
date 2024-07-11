#include "mainwindow.h"
#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <log.hpp>
#include "./ui_mainwindow.h"

bool MainWindow::initialize() {
  if (std::filesystem::exists(m_fifo_path)) {
    LOG_DEBUG("removing existing fifo before creating new one");
    std::error_code ec;
    std::filesystem::remove(m_fifo_path, ec);
    if (ec) {
      LOG_ERROR("failed removing fifo: {}", ec.message());
      return false;
    }
  }

  m_decoder = make_decoder();
  if (!m_decoder) {
    LOG_ERROR("failed creating decoder");
    return false;
  }

  m_udp_receive = make_udp_receive(m_ctx, 34000);
  if (!m_udp_receive) {
    LOG_ERROR("failed creating udp receive");
    return false;
  }

  m_fifo_fd = mkfifo(m_fifo_path.c_str(),
                     S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
  if (m_fifo_fd < 0) {
    std::cerr << "ERROR: failed opening fifo: " << strerror(errno) << "\n";
    return false;
  }

  // We are going to use non-blocking IO to be able to unblock the thread
  // anytime we want to close the window.
  if (int res = ::fcntl(m_fifo_fd, F_SETFL, O_NONBLOCK); res < 0) {
    std::cerr << "ERROR: fcntl failed: " << strerror(errno) << "\n";
    return false;
  }

  m_readbuff.resize(get_framebuff_size());
  assert(m_readbuff.size() == get_framebuff_size());

  return true;
}

void MainWindow::start() {
  m_stdin_read_th = std::jthread{[this](std::stop_token tok) {
    struct pollfd poll_fds[1] = {{fd : m_fifo_fd, events : POLLIN}};

    while (!tok.stop_requested()) {
      int res = ::poll(poll_fds, 1, 100);
      if (tok.stop_requested()) {
        break;
      }
      if (res == 0) {
        std::cout << "DEBUG: poll timeout\n";
        continue;
      } else if (res < 0) {
        std::cerr << "ERROR: poll failed: " << strerror(errno) << "\n";
        break;
      } else {
        if (poll_fds[0].revents & POLLIN) {
          std::cout << "DEBUG: READ ready\n";
        } else {
          std::cout << "WARNING: unsupported event\n";
          break;
        }
      }
      const ssize_t read_res =
          read(m_fifo_fd, m_readbuff.data() + m_readbuff_size,
               m_readbuff.size() - m_readbuff_size);
      if (read_res < 0) {
        std::cerr << "ERROR read failed: " << strerror(errno) << "\n";
        break;
      } else if (read_res == 0) {
        std::cerr << "ERROR: eof\n";
        break;
      } else {
        std::cout << "DEBUG: bytes_read: " << read_res << "\n";
        m_readbuff_size += read_res;
      }
    }
    std::cout << "DEBUG: stdin reading thread stopped\n";
  }};

  m_udp_receive->start(*this);
}

void MainWindow::stop() {
  if (m_stdin_read_th.joinable()) {
    m_stdin_read_th.request_stop();
    std::cout << "DEBUG: closing input descriptor\n";
    m_stdin_read_th.join();
  }
}

void MainWindow::on_packet_received(VideoPacket p) /*override*/ {
  LOG_DEBUG("Got a packet");
}

MainWindow::MainWindow(asio::io_context& ctx,
                       int width,
                       int height,
                       std::string pixelformat,
                       std::string fifo_path,
                       QWidget* parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), m_ctx{ctx} {
  m_width = width;
  m_height = height;
  m_pixformat = pixelformat;
  m_fifo_path = fifo_path;
  ui->setupUi(this);
}

MainWindow::~MainWindow() {
  delete ui;
}
