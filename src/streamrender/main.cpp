#include "mainwindow.h"

#include <QApplication>
#include <cstdlib>
#include <iostream>

int main(int argc, char* argv[]) {
  if (argc != 5) {
    std::cerr << "ERROR: no arguments specified.\nUSAGE: " << argv[0]
              << " <WIDTH> <HEIGHT> <PIXFORMAT> <FIFO_PATH>\n";
    return -1;
  }
  QApplication a(argc, argv);
  MainWindow w{std::atoi(argv[1]), std::atoi(argv[2]), argv[3], argv[4]};
  if (!w.initialize()) {
    std::cerr << "ERROR: failed to initialize application\n";
    return -1;
  }
  w.start();
  w.show();
  return a.exec();
}
