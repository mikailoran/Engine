#include "editor/main_window.h"

#include <QAction>
#include <QKeySequence>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QWidget>
#include <memory>

#include "editor/viewport_window.h"

namespace editor {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  setWindowTitle(tr("Scene Editor"));
  resize(400, 400);

  QMenu* file_menu = menuBar()->addMenu(tr("&File"));
  QAction* quit = file_menu->addAction(tr("&Quit"));
  quit->setShortcut(QKeySequence::Quit);
  connect(quit, &QAction::triggered, this, [this]() -> void { close(); });

  // The container takes ownership of the viewport and lays it out like a widget
  auto viewport = std::make_unique<ViewportWindow>();
  setCentralWidget(QWidget::createWindowContainer(viewport.release(), this));
}

}  // namespace editor
