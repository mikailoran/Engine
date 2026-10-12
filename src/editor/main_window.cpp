#include "editor/main_window.h"

#include <QtCore/qnamespace.h>

#include <QAction>
#include <QFileDialog>
#include <QKeySequence>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QString>
#include <QWidget>
#include <exception>
#include <filesystem>
#include <string>
#include <string_view>

#include "editor/viewport_window.h"

namespace editor {

namespace {

// The source tree's scenes, so edits show without a rebuild
constexpr std::string_view kSceneDir = EDITOR_SCENE_DIR;
// The launcher's entry scene
constexpr std::string_view kStartScene = "level02.json";

}  // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), viewport_(new ViewportWindow) {
  setWindowTitle(tr("Scene Editor"));
  resize(1280, 720);

  QMenu* file_menu = menuBar()->addMenu(tr("&File"));
  QAction* open = file_menu->addAction(tr("&Open Scene..."));
  open->setShortcut(QKeySequence::Open);
  connect(open, &QAction::triggered, this, &MainWindow::ChooseScene);

  reload_ = file_menu->addAction(tr("&Reload Scene"));
  reload_->setShortcut(QKeySequence::Refresh);
  reload_->setEnabled(false);
  connect(reload_, &QAction::triggered, this,
          [this]() -> void { OpenScene(scene_); });

  file_menu->addSeparator();
  QAction* quit = file_menu->addAction(tr("&Quit"));
  quit->setShortcut(QKeySequence::Quit);
  connect(quit, &QAction::triggered, this, &MainWindow::close);

  // The container takes ownership of the viewport and lays it out like a widget
  QWidget* container = QWidget::createWindowContainer(viewport_, this);
  // Clicking or tabbing to the viewport gives the game the keyboard
  container->setFocusPolicy(Qt::StrongFocus);
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
  // Remembered before loading, so a broken file can be fixed and reloaded
  scene_ = path;
  reload_->setEnabled(true);
  setWindowTitle(tr("%1 - Scene Editor")
                     .arg(QString::fromStdString(path.filename().string())));
  try {
    viewport_->LoadScene(path);
  } catch (const std::exception& e) {
    QMessageBox::warning(this, tr("Cannot open scene"),
                         QString::fromStdString(e.what()));
  }
}

void MainWindow::ChooseScene() {
  const QString file = QFileDialog::getOpenFileName(
      this, tr("Open Scene"), QString::fromStdString(std::string(kSceneDir)),
      tr("Scenes (*.json)"));
  // Empty when the dialog was cancelled
  if (!file.isEmpty()) {
    OpenScene(file.toStdString());
  }
}

}  // namespace editor
