#include "mainwindow.h"

#include <QApplication>
#include <asio/io_context.hpp>
#include <cstdlib>
#include <iostream>
#include <log.hpp>
#include <thread>

int main(int argc, char* argv[]) {
  if (argc != 5) {
    std::cerr << "ERROR: no arguments specified.\nUSAGE: " << argv[0]
              << " <WIDTH> <HEIGHT> <PIXFORMAT> <FIFO_PATH>\n";
    return -1;
  }

  asio::io_context ctx;
  std::jthread asio_thread{[&ctx] {
    LOG_DEBUG("Starting asio thread..");
    asio::io_context::work dummy_work{ctx};
    ctx.run();
    LOG_DEBUG("asio thread stopped");
  }};

  QApplication a(argc, argv);
  MainWindow w{ctx, std::atoi(argv[1]), std::atoi(argv[2]), argv[3], argv[4]};
  if (!w.initialize()) {
    std::cerr << "ERROR: failed to initialize application\n";
    return -1;
  }

  w.start();
  w.show();
  int res = a.exec();

  ctx.stop();  // jthread will be autojoined

  return res;
}
