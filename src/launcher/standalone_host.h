#pragma once

#include <cstdint>

#include "devtools/fly_camera_system.h"
#include "devtools/ui.h"
#include "engine/ecs/core/types.h"
#include "engine/engine.h"
#include "engine/platform/input.h"
#include "engine/platform/screen.h"
#include "game/game_session.h"
#include "launcher/window.h"

namespace launcher {

/**
 * @brief The standalone host: the SDL window and input, the frame loop, the
 * engine, one game session, and devtools.
 *
 * Member order is the bring-up order; destruction runs in reverse. The asset
 * root must be set before construction.
 */
class StandaloneHost {
 public:
  /**
   * @brief Opens the window, brings up the engine and devtools, and loads the
   * entry scene.
   * @throws std::runtime_error If the window, engine or scene fails to load.
   */
  StandaloneHost();

  /**
   * @brief Runs the frame loop until the window closes.
   * @return Process exit code.
   */
  auto Run() -> int;

 private:
  /** @brief Who has the mouse and keyboard: the player, or the debug tools. */
  enum class Mode : std::uint8_t { kPlay, kDebug };

  /// Window size in screen points at startup.
  static constexpr engine::PixelSize kStartSize{.width = 1280, .height = 720};

  /** @brief Switches to @p mode: captures or frees the mouse, picks a camera.
   */
  void EnterMode(Mode mode);

  Window window_{kStartSize};
  engine::Input input_;

  engine::Engine engine_{window_.Surface()};
  game::GameSession session_{engine_};

  // After the engine: UiSystem's GPU resources must go before bgfx does
  devtools::FlyCameraSystem fly_camera_;
  devtools::UiSystem ui_;

  engine::Entity fly_camera_entity_{};
  Mode mode_{Mode::kPlay};
};

}  // namespace launcher
