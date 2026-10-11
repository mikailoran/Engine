#include "editor/main_window.h"

#include <QAction>
#include <QKeySequence>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QWidget>

namespace editor {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  setWindowTitle(tr("Scene Editor"));
  resize(400, 400);

  QMenu* file_menu = menuBar()->addMenu(tr("&File"));
  QAction* quit = file_menu->addAction(tr("&Quit"));
  quit->setShortcut(QKeySequence::Quit);
  connect(quit, &QAction::triggered, this, [this]() -> void { close(); });
}

}  // namespace editor
