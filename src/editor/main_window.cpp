#include "editor/main_window.h"

#include <QtCore/qnamespace.h>

#include <QAction>
#include <QKeySequence>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QString>
#include <QWidget>
#include <exception>
#include <filesystem>
#include <memory>
#include <string_view>

#include "editor/viewport_window.h"

namespace editor {

namespace {

// The source tree's scenes, so edits show without a rebuild
constexpr std::string_view kSceneDir = EDITOR_SCENE_DIR;
// The launcher's entry scene
constexpr std::string_view kStartScene = "level02.json";

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  setWindowTitle(tr("Scene Editor"));
  resize(400, 400);

  QMenu* file_menu = menuBar()->addMenu(tr("&File"));
  QAction* quit = file_menu->addAction(tr("&Quit"));
  quit->setShortcut(QKeySequence::Quit);
  connect(quit, &QAction::triggered, this, [this]() -> void { close(); });

  // The container takes ownership of the viewport and lays it out like a widget
  auto viewport = std::make_unique<ViewportWindow>();
  viewport_ = viewport.get();
  QWidget* container = QWidget::createWindowContainer(viewport.release(), this);
  // Clicking or tabbing to the viewport gives the game the keyboard
  container->setFocusPolicy(Qt::StrongFocus);
  // On Wayland the keyboard reaches the container, not the window inside it
  container->installEventFilter(viewport_);
  setCentralWidget(container);
  container->setFocus();

  // Queued: load after the expose event that created the engine has returned
  connect(
      viewport_, &ViewportWindow::Ready, this,
      [this]() -> void {
        OpenScene(std::filesystem::path(kSceneDir) / kStartScene);
      },
      Qt::QueuedConnection);
}

void MainWindow::OpenScene(const std::filesystem::path& path) {
  try {
    viewport_->LoadScene(path);
  } catch (const std::exception& e) {
    QMessageBox::warning(this, tr("Cannot open scene"),
                         QString::fromStdString(e.what()));
  }
}

}  // namespace editor
