#include <chrono>
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
using engine::PixelSize;
using engine::RenderOptions;
using engine::SetAssetRoot;
using engine::Transform;

namespace game {

namespace {

// Window size in screen points at startup
constexpr PixelSize kStartSize{.width = 1280, .height = 720};

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
  Window window_{kStartSize};
  Input input_;

  Engine engine_{window_.Surface()};

  // After the engine: UiSystem's GPU resources must go before bgfx does
  FlyCameraSystem fly_camera_;
  UiSystem ui_;
};

Game::Game() {
  Ecs& ecs = engine_.World();
  ecs.RegisterComponent<Selected>();

  const auto camera_entity = ecs.CreateEntity();
  ecs.AddComponent(camera_entity, Transform{.position = {0.0F, 1.0F, 5.0F}});
  ecs.AddComponent(camera_entity, Camera{});
  engine_.SetActiveCamera(camera_entity);
  fly_camera_.SetControlledCamera(camera_entity);

  // Load the debug scene's decor as ordinary entities
  engine_.LoadScene("assets/scenes/debug.json");
}

auto Game::Run() -> int {
  using Clock = std::chrono::steady_clock;
  using Seconds = std::chrono::duration<float>;
  const auto start_time = Clock::now();
  auto last_time = start_time;

  while (!window_.PumpEvents(input_)) {
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

    // The camera pose must settle before the engine reads it.
    fly_camera_.Update(engine_.World(), ctx, ui_.WantsMouse(),
                       ui_.WantsKeyboard());
    // UI runs last, so this is last frame's answer
    UpdateSelection(engine_, ctx, ui_.WantsMouse());
    engine_.Update(ctx);

    const RenderOptions render_options{.debug_draw = ui_.DebugDrawEnabled()};
    if (render_options.debug_draw) {
      // Selection is devtools policy; the engine only draws what it's asked
      engine_.World().View<Selected>().ForEach(
          [this](Entity entity, const Selected& /*selected*/) -> void {
            engine_.Highlight(entity);
          });
    }
    engine_.Render(ctx, render_options);
    ui_.Update(engine_.World(), engine_.Assets(), ctx);
    // Typed text only flows while a UI text field asks for it
    window_.SetTextInput(ui_.WantsText());

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
