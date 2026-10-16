#include "launcher/standalone_host.h"

#include <chrono>

#include "devtools/selected.h"
#include "devtools/selection_system.h"
#include "engine/ecs/components/camera.h"
#include "engine/ecs/components/transform.h"
#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"
#include "engine/engine.h"
#include "engine/platform/frame_context.h"
#include "engine/platform/input.h"
#include "engine/platform/screen.h"
#include "game/game_logic.h"

using devtools::Selected;
using devtools::UpdateSelection;
using engine::Camera;
using engine::Ecs;
using engine::Entity;
using engine::FrameContext;
using engine::Key;
using engine::PixelSize;
using engine::RenderOptions;
using engine::Transform;
using game::RegisterGameLogic;

namespace launcher {

StandaloneHost::StandaloneHost() {
  Ecs& ecs = engine_.World();
  ecs.RegisterComponent<Selected>();
  RegisterGameLogic(engine_);

  fly_camera_entity_ = ecs.CreateEntity();
  ecs.AddComponent(fly_camera_entity_,
                   Transform{.position = {0.0F, 1.0F, 5.0F}});
  ecs.AddComponent(fly_camera_entity_, Camera{});
  fly_camera_.SetControlledCamera(fly_camera_entity_);

  // The level and its props are ordinary entities
  engine_.LoadScene("assets/scenes/level02.json");
  EnterMode(Mode::kPlay);
}

void StandaloneHost::EnterMode(Mode mode) {
  mode_ = mode;
  const bool play = mode == Mode::kPlay;
  window_.SetRelativeMouse(play);
  engine_.SetActiveCamera(play ? session_.ViewCamera() : fly_camera_entity_);
}

auto StandaloneHost::Run() -> int {
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
    session_.PrePhysics(engine_, ctx, play);
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
    session_.PostPhysics(engine_);

    // Playing shows the game alone, without wireframes or the inspector
    const RenderOptions render_options{
        .debug_draw = !play && ui_.DebugDrawEnabled(), .hud = play};
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

}  // namespace launcher
