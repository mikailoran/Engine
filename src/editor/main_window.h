#pragma once

#include <QtCore/qtclasshelpermacros.h>

#include <QMainWindow>
#include <QTimer>
#include <chrono>
#include <filesystem>
#include <optional>

#include "engine/engine.h"
#include "game/game_session.h"

class QAction;

namespace editor {

class ViewportWindow;

/**
 * @brief The editor's top-level window: the menus, the viewport, and the
 * engine and game session it renders.
 *
 * The engine is created on the viewport's first expose, when its native
 * surface exists, and destroyed just before Qt destroys that surface. Opens
 * the launcher's start scene once the engine exists; scenes are opened from
 * the source tree's `assets/scenes/`.
 */
class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  /**
   * @brief Builds the menus and embeds a new viewport as the central widget.
   * Requires a QApplication. The engine waits for the viewport's first expose.
   */
  explicit MainWindow(QWidget* parent = nullptr);

  /**
   * @brief Disconnects the viewport, which outlives these members: Qt
   * destroys it with the central widget, and it signals SurfaceLost then.
   */
  ~MainWindow() override;

  Q_DISABLE_COPY_MOVE(MainWindow)

 private:
  using Clock = std::chrono::steady_clock;

  /**
   * @brief Creates the engine on the first expose, then queues the start
   * scene. Ticks only while the viewport is on screen.
   */
  void OnViewportExposed(bool on_screen);

  /** @brief Destroys the session and engine while the surface still exists. */
  void OnViewportSurfaceLost();

  /**
   * @brief Replaces the world with a scene file's entities and starts a new
   * game session on it. Requires the engine.
   * @param path Absolute, or relative to the asset root.
   * @throws std::runtime_error If the scene or an asset it names won't load;
   *         the world then holds whatever loaded before the failure.
   */
  void LoadScene(const std::filesystem::path& path);

  /** @brief Runs one frame of the game: gameplay, physics, then render. */
  void Tick();

  /**
   * @brief Loads @p path into the viewport, or shows why it couldn't. A failed
   * load leaves the editor running, and Reload tries the same file again.
   */
  void OpenScene(const std::filesystem::path& path);

  /** @brief Asks for a scene file in the source tree, then opens it. */
  void ChooseScene();

  // Owned by the central widget's window container.
  ViewportWindow* viewport_{nullptr};

  // Owned by the File menu; disabled until a scene has been opened.
  QAction* reload_{nullptr};

  // The scene last opened, which Reload loads again.
  std::filesystem::path scene_;

  // Engine before session: a session runs on the engine
  std::optional<engine::Engine> engine_;
  // Empty until the first scene loads
  std::optional<game::GameSession> session_;

  // Fires as often as the event loop allows; bgfx's vsync paces the frames
  QTimer frame_timer_;
  Clock::time_point start_time_;
  Clock::time_point last_time_;
};

}  // namespace editor
