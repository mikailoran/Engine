#include <chrono>
#include <cstdint>
#include <exception>
#include <iostream>

#include "devtools/fly_camera_system.h"
#include "devtools/selected.h"
#include "devtools/selection_system.h"
#include "devtools/ui.h"
#include "engine/ecs/components/camera.h"
#include "engine/ecs/components/transform.h"
#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"
#include "engine/engine.h"
#include "engine/platform/asset_root.h"
#include "engine/platform/bgfx_file_access.h"
#include "engine/platform/frame_context.h"
#include "engine/platform/input.h"
#include "engine/platform/screen.h"
#include "game/logic/game_logic.h"
#include "game/logic/player_system.h"
#include "game/window.h"

using devtools::FlyCameraSystem;
using devtools::Selected;
using devtools::UiSystem;
using devtools::UpdateSelection;
using engine::AssetRoot;
using engine::Camera;
using engine::Ecs;
using engine::Engine;
using engine::Entity;
using engine::FrameContext;
using engine::Input;
using engine::Key;
using engine::PixelSize;
using engine::RenderOptions;
using engine::SetAssetRoot;
using engine::Transform;

namespace game {

namespace {

// Window size in screen points at startup
constexpr PixelSize kStartSize{.width = 1280, .height = 720};

/** @brief Who has the mouse and keyboard: the player, or the debug tools. */
enum class Mode : std::uint8_t { kPlay, kDebug };

/**
 * @brief The game host: the SDL window and input, the engine, and devtools.
 *
 * Member order is the bring-up order; destruction runs in reverse. The asset
 * root must be set before construction.
 */
class Game {
 public:
  /** @brief Opens the window, then brings up the engine and devtools. */
  Game();

  /**
   * @brief Runs the frame loop until the window closes.
   * @return Process exit code.
   */
  auto Run() -> int;

 private:
  /** @brief Switches to @p mode: captures or frees the mouse, picks a camera.
   */
  void EnterMode(Mode mode);

  Window window_{kStartSize};
  Input input_;

  Engine engine_{window_.Surface()};
  PlayerSystem player_;

  // After the engine: UiSystem's GPU resources must go before bgfx does
  FlyCameraSystem fly_camera_;
  UiSystem ui_;

  Entity player_camera_{};
  Entity fly_camera_entity_{};
  Mode mode_{Mode::kPlay};
};

Game::Game() {
  Ecs& ecs = engine_.World();
  ecs.RegisterComponent<Selected>();
  RegisterGameLogic(engine_);

  // PlayerSystem moves this one to the player's eyes
  player_camera_ = ecs.CreateEntity();
  ecs.AddComponent(player_camera_, Transform{});
  ecs.AddComponent(player_camera_, Camera{});
  player_.SetViewCamera(player_camera_);

  fly_camera_entity_ = ecs.CreateEntity();
  ecs.AddComponent(fly_camera_entity_,
                   Transform{.position = {0.0F, 1.0F, 5.0F}});
  ecs.AddComponent(fly_camera_entity_, Camera{});
  fly_camera_.SetControlledCamera(fly_camera_entity_);

  // The level and its props are ordinary entities
  engine_.LoadScene("assets/scenes/level01.json");
  EnterMode(Mode::kPlay);
}

void Game::EnterMode(Mode mode) {
  mode_ = mode;
  const bool play = mode == Mode::kPlay;
  window_.SetRelativeMouse(play);
  engine_.SetActiveCamera(play ? player_camera_ : fly_camera_entity_);
}

auto Game::Run() -> int {
  using Clock = std::chrono::steady_clock;
  using Seconds = std::chrono::duration<float>;
  const auto start_time = Clock::now();
  auto last_time = start_time;

  while (!window_.PumpEvents(input_)) {
    if (input_.Pressed(Key::kF1)) {
      EnterMode(mode_ == Mode::kPlay ? Mode::kDebug : Mode::kPlay);
    }
    const bool play = mode_ == Mode::kPlay;

    const auto now = Clock::now();
    const float dt = Seconds(now - last_time).count();
    last_time = now;

    // Resizes and display-scale changes both land here
    const PixelSize size = window_.BackbufferSize();
    engine_.Resize(size);

    // One context per frame, shared by every system.
    const FrameContext ctx{
        .width = size.width,
        .height = size.height,
        .dt = dt,
        .time = Seconds(now - start_time).count(),
        .input = input_,
    };

    // Input moves the player before physics; the debug tools only in debug
    player_.Control(engine_.World(), ctx, play);
    if (!play) {
      // The camera pose must settle before the engine reads it.
      fly_camera_.Update(engine_.World(), ctx, ui_.WantsMouse(),
                         ui_.WantsKeyboard());
      // UI runs last, so this is last frame's answer
      UpdateSelection(engine_, ctx, ui_.WantsMouse());
    }
    if (play) {
      engine_.Update(ctx);
    }
    // After physics, so the view doesn't trail the player by a frame
    player_.PlaceCamera(engine_.World());

    // Playing shows the game alone, without wireframes or the inspector
    const RenderOptions render_options{.debug_draw =
                                           !play && ui_.DebugDrawEnabled()};
    if (render_options.debug_draw) {
      // Selection is devtools policy; the engine only draws what it's asked
      engine_.World().View<Selected>().ForEach(
          [this](Entity entity, const Selected& /*selected*/) -> void {
            engine_.Highlight(entity);
          });
    }
    engine_.Render(ctx, render_options);
    if (!play) {
      ui_.Update(engine_.World(), engine_.Assets(), ctx);
    }
    // Typed text only flows while a UI text field asks for it
    window_.SetTextInput(!play && ui_.WantsText());

    engine_.EndFrame();
  }

  return 0;
}

}  // namespace

}  // namespace game

/** @brief Sets the asset root, then runs the game until its window closes. */
auto main() -> int {
  // bgfx_utils prepends this to every asset path; must precede any load
  SetAssetRoot(AssetRoot());

  try {
    game::Game game;
    return game.Run();
  } catch (const std::exception& e) {
    std::cerr << "fatal: " << e.what() << '\n';
    return 1;
  }
}
