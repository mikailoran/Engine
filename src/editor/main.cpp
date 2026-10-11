#include <QApplication>

#include "editor/main_window.h"

auto main(int argc, char* argv[]) -> int {
  const QApplication app(argc, argv);
  editor::MainWindow window;
  window.show();
  return QApplication::exec();
}
