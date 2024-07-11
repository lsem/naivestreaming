#include "mainwindow.h"
#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <QPainter>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <log.hpp>
#include "./ui_mainwindow.h"

bool MainWindow::initialize() {
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

void MainWindow::paintEvent(QPaintEvent* event) /*override*/ {
  QPainter painter;
  painter.begin(this);

  srand(42);
  QBrush brush{QColor{135, 135, 135}};
  for (int i = 0; i < m_packets_received; ++i) {
    painter.fillRect(QRectF(rand() % width(), rand() % height(), 5, 5), brush);
  }

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
