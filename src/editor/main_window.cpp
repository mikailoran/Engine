#include "editor/main_window.h"

#include <QtCore/qnamespace.h>

#include <QAbstractItemView>
#include <QAction>
#include <QCoreApplication>
#include <QDebug>
#include <QDockWidget>
#include <QFileDialog>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLabel>
#include <QListView>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMetaObject>
#include <QObject>
#include <QSplitter>
#include <QString>
#include <QWidget>
#include <QtLogging>
#include <cassert>
#include <chrono>
#include <exception>
#include <filesystem>
#include <string>
#include <string_view>

#include "editor/entity_list_model.h"
#include "editor/inspector_widget.h"
#include "editor/viewport_window.h"
#include "engine/ecs/components/transform.h"
#include "engine/ecs/core/types.h"
#include "engine/engine.h"
#include "engine/platform/frame_context.h"
#include "engine/platform/screen.h"
#include "game/game_logic.h"

using engine::Engine;
using engine::FrameContext;
using engine::PixelSize;

namespace editor {

namespace {

// The source tree's scenes, so edits show without a rebuild
constexpr std::string_view kSceneDir = EDITOR_SCENE_DIR;
// The launcher's entry scene
constexpr std::string_view kStartScene = "level02.json";

}  // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      viewport_(new ViewportWindow),
      entities_(new EntityListModel(this)) {
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

  // View dock
  auto* dock_left = new QDockWidget(tr("Entities"), this);
  MainWindow::addDockWidget(Qt::LeftDockWidgetArea, dock_left);
  auto* dock_left_splitter =
      new QSplitter(Qt::Orientation::Vertical, dock_left);
  dock_left->setWidget(dock_left_splitter);

  auto* entity_list_view = new QListView(dock_left_splitter);
  entity_list_view->setModel(entities_);
  entity_list_view->setSelectionMode(QAbstractItemView::SingleSelection);
  entity_list_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
  entity_list_view->setUniformItemSizes(true);
  dock_left_splitter->addWidget(entity_list_view);

  inspector_ = new InspectorWidget(dock_left_splitter);
  dock_left_splitter->addWidget(inspector_);

  const auto* view_selection_model = entity_list_view->selectionModel();
  connect(view_selection_model, &QItemSelectionModel::selectionChanged, this,
          &MainWindow::UpdateInspectorFromSelection);

  // The container takes ownership of the viewport and lays it out like a widget
  QWidget* container = QWidget::createWindowContainer(viewport_, this);
  // Clicking or tabbing to the viewport gives the game the keyboard
  container->setFocusPolicy(Qt::StrongFocus);
  setCentralWidget(container);
  container->setFocus();

  connect(viewport_, &ViewportWindow::Exposed, this,
          &MainWindow::OnViewportExposed);
  // Direct: the engine must go before the surface does
  connect(viewport_, &ViewportWindow::SurfaceLost, this,
          &MainWindow::OnViewportSurfaceLost, Qt::DirectConnection);
  connect(&frame_timer_, &QTimer::timeout, this, &MainWindow::Tick);
}

MainWindow::~MainWindow() {
  // The engine goes with the members, still before the surface
  QObject::disconnect(viewport_, nullptr, this, nullptr);
}

void MainWindow::OnViewportExposed(bool on_screen) {
  if (!on_screen) {
    frame_timer_.stop();
    return;
  }
  if (!engine_) {
    try {
      engine_.emplace(viewport_->Surface());
      game::RegisterGameLogic(*engine_);
    } catch (const std::exception& e) {
      // Exceptions must not cross Qt's event loop
      qCritical() << "fatal:" << e.what();
      engine_.reset();
      QCoreApplication::exit(1);
      return;
    }
    start_time_ = Clock::now();
    last_time_ = start_time_;
    // Queued: load after the expose event that created the engine has returned
    QMetaObject::invokeMethod(
        this,
        [this]() -> void {
          OpenScene(std::filesystem::path(kSceneDir) / kStartScene);
        },
        Qt::QueuedConnection);
  }
  frame_timer_.start();
}

void MainWindow::OnViewportSurfaceLost() {
  frame_timer_.stop();
  session_.reset();
  engine_.reset();
}

void MainWindow::LoadScene(const std::filesystem::path& path) {
  assert(engine_ && "LoadScene before the engine exists");
  Engine& engine = engine_.value();
  engine.ClearWorld();
  session_.emplace(engine);
  engine.SetActiveCamera(session_->ViewCamera());
  engine.LoadScene(path);
  // Loading time is not game time
  last_time_ = Clock::now();
}

void MainWindow::Tick() {
  // Nothing to show until a scene has started a session
  if (!engine_ || !session_) {
    return;
  }
  const PixelSize size = viewport_->BackbufferSize();
  if (size.width == 0 || size.height == 0) {
    return;
  }

  using Seconds = std::chrono::duration<float>;
  const auto now = Clock::now();
  const float dt = Seconds(now - last_time_).count();
  last_time_ = now;

  engine_->Resize(size);
  const FrameContext ctx{
      .width = size.width,
      .height = size.height,
      .dt = dt,
      .time = Seconds(now - start_time_).count(),
      .input = viewport_->TakeFrameInput(),
  };

  // The launcher's play-mode frame, without devtools
  session_->PrePhysics(*engine_, ctx, /*has_control=*/true);
  engine_->Update(ctx);
  session_->PostPhysics(*engine_);
  engine_->Render(ctx, {.debug_draw = false});
  engine_->EndFrame(ctx);
}

void MainWindow::OpenScene(const std::filesystem::path& path) {
  // Remembered before loading, so a broken file can be fixed and reloaded
  scene_ = path;
  reload_->setEnabled(true);
  setWindowTitle(tr("%1 - Scene Editor")
                     .arg(QString::fromStdString(path.filename().string())));
  try {
    LoadScene(path);
  } catch (const std::exception& e) {
    QMessageBox::warning(this, tr("Cannot open scene"),
                         QString::fromStdString(e.what()));
  }
  if (engine_) {
    entities_->Refresh(engine_.value().World());
  }
  // The model reset drops the selection without emitting selectionChanged
  inspector_->Clear();
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

auto MainWindow::UpdateInspectorFromSelection(const QItemSelection& selected)
    -> void {
  if (!engine_) {
    return;
  }

  // Handle deselection case.
  if (selected.empty()) {
    inspector_->SetDiagnostic(InspectorWidget::Diagnostic::kNoSelection);
    return;
  }

  const auto index = selected.indexes().value(0);
  const auto entity =
      // NOLINTNEXTLINE(clazy-qvariant-template-instantiation)
      index.data(EntityListModel::kEntityRole).value<engine::Entity>();
  const auto name = index.data().toString();

  const auto* transform =
      engine_->World().TryGetComponent<engine::Transform>(entity);

  if (transform) {
    inspector_->UpdateTransform(name, *transform);
  } else {
    inspector_->SetDiagnostic(InspectorWidget::Diagnostic::kNoTransform);
  }
}

}  // namespace editor
