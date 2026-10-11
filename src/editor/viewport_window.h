#pragma once

#include <QTimer>
#include <QWindow>
#include <chrono>
#include <filesystem>
#include <optional>

#include "engine/engine.h"
#include "engine/platform/native_surface.h"
#include "engine/platform/screen.h"
#include "game/game_session.h"

class QEvent;
class QExposeEvent;

namespace editor {

/**
 * @brief The native window the engine renders the game into, and the engine
 * and game session it hosts.
 *
 * Embedded in the main window through QWidget::createWindowContainer, which
 * takes ownership of it. Qt never paints it: bgfx presents to it directly. The
 * engine is created on the first expose, when the native surface exists, and
 * destroyed just before Qt destroys that surface.
 */
class ViewportWindow : public QWindow {
  Q_OBJECT

 public:
  /** @brief Creates the window as a Vulkan surface, for bgfx to present to. */
  explicit ViewportWindow(QWindow* parent = nullptr);

  /**
   * @brief Replaces the world with a scene file's entities and starts a new
   * game session on it. After Ready.
   * @param path Absolute, or relative to the asset root.
   * @throws std::runtime_error If the scene or an asset it names won't load;
   *         the world then holds whatever loaded before the failure.
   */
  void LoadScene(const std::filesystem::path& path);

 signals:
  /**
   * @brief The engine exists; scenes can be loaded. Emitted on the first
   * expose, and again if Qt destroyed and recreated the surface.
   */
  void Ready();

 protected:
  /** @brief Creates the engine on the first expose; ticks only while shown. */
  void exposeEvent(QExposeEvent* event) override;

  /** @brief Destroys the engine before Qt destroys the native surface. */
  auto event(QEvent* e) -> bool override;

 private:
  using Clock = std::chrono::steady_clock;

  /** @brief Runs one frame of the game: gameplay, physics, then render. */
  void Tick();

  /**
   * @brief The native handles and backbuffer size, for the engine. Valid once
   * the window has been exposed.
   * @throws std::runtime_error If Qt runs on neither Wayland nor xcb, or a
   *         handle is missing.
   */
  [[nodiscard]] auto Surface() const -> engine::NativeSurface;

  /** @brief Current size in device pixels: logical size times the scale. */
  [[nodiscard]] auto BackbufferSize() const -> engine::PixelSize;

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
