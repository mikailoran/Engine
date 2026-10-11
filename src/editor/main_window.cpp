#include "editor/main_window.h"

#include <QMainWindow>
#include <QWidget>

namespace editor {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  setWindowTitle(tr("Scene Editor"));
  resize(200, 200);
}

}  // namespace editor
