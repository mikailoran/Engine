#include <bgfx/bgfx.h>
#include <bx/timer.h>
#include <common.h>
#include <entry/entry.h>
#include <entry/input.h>

#include <array>
#include <cstdint>
#include <exception>
#include <iostream>

#include "devtools/fly_camera_system.h"
#include "devtools/selected.h"
#include "devtools/selection_system.h"
#include "devtools/ui.h"
#include "engine/bgfx_context.h"
#include "engine/ecs/components/camera.h"
#include "engine/ecs/components/transform.h"
#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"
#include "engine/engine.h"
#include "engine/platform/asset_root.h"
#include "engine/platform/frame_context.h"
#include "engine/platform/input.h"
#include "engine/platform/native_surface.h"

namespace {

/** @brief An engine key and the entry key that drives it. */
struct KeyBinding {
  Key key;
  entry::Key::Enum entry_key;
};

constexpr std::array<KeyBinding, 6> kKeyBindings{{
    {.key = Key::kW, .entry_key = entry::Key::KeyW},
    {.key = Key::kA, .entry_key = entry::Key::KeyA},
    {.key = Key::kS, .entry_key = entry::Key::KeyS},
    {.key = Key::kD, .entry_key = entry::Key::KeyD},
    {.key = Key::kQ, .entry_key = entry::Key::KeyQ},
    {.key = Key::kE, .entry_key = entry::Key::KeyE},
}};

/**
 * @brief Copies entry's mouse and key state into @p input as a new frame.
 * @param last_scroll entry's scroll total last frame; updated to this one's.
 */
void ReadEntryInput(const entry::MouseState& mouse, std::int32_t& last_scroll,
                    InputState& input) {
  input.BeginFrame();
  input.SetMouse({.x = static_cast<float>(mouse.m_mx),
                  .y = static_cast<float>(mouse.m_my)});
  input.SetButton(MouseButton::kLeft,
                  mouse.m_buttons[entry::MouseButton::Left] != 0);
  input.SetButton(MouseButton::kRight,
                  mouse.m_buttons[entry::MouseButton::Right] != 0);
  input.SetButton(MouseButton::kMiddle,
                  mouse.m_buttons[entry::MouseButton::Middle] != 0);

  // entry reports a running total; InputState wants this frame's notches
  input.AddWheel(static_cast<float>(mouse.m_mz - last_scroll));
  last_scroll = mouse.m_mz;

  for (const KeyBinding& binding : kKeyBindings) {
    input.SetKey(binding.key, inputGetKeyState(binding.entry_key));
  }
}

/** @brief Window size and bgfx flags that entry reads and writes back. */
struct WindowState {
  uint32_t width = 1280;
  uint32_t height = 720;
  // entry applies these itself on resize and debug-key toggles
  uint32_t debug = BgfxContext::kDebugFlags;
  uint32_t reset = BgfxContext::kResetFlags;
};

/** @brief Describes the window entry created, for the engine to render into. */
auto EntrySurface(const WindowState& window) -> NativeSurface {
  const bool wayland = entry::getNativeWindowHandleType() ==
                       bgfx::NativeWindowHandleType::Wayland;
  return {
      .window = entry::getNativeWindowHandle(entry::kDefaultWindowHandle),
      .display = entry::getNativeDisplayHandle(),
      .kind = wayland ? SurfaceKind::kWayland : SurfaceKind::kX11,
      .width = window.width,
      .height = window.height,
  };
}

/**
 * @brief The game host: entry's window and input, the engine, and devtools.
 *
 * Member order is the bring-up order; destruction runs in reverse. The asset
 * root must be set before construction.
 */
class Game {
 public:
  /** @brief Brings up the engine, the devtools and the debug scene. */
  Game();

  /**
   * @brief Runs the frame loop until the window closes.
   * @return Process exit code.
   */
  auto Run() -> int;

 private:
  WindowState window_;
  entry::MouseState mouse_state_;
  // entry's scroll total last frame, to turn it into per-frame notches.
  std::int32_t last_scroll_{0};
  InputState input_;
  FrameTime frame_time_;

  Engine engine_{EntrySurface(window_)};

  // After the engine: UiSystem's GPU resources must go before bgfx does
  FlyCameraSystem fly_camera_;
  UiSystem ui_;
};

Game::Game() {
  Ecs& ecs = engine_.World();
  ecs.RegisterComponent<Selected>();

  const auto camera_entity = ecs.CreateEntity();
  ecs.AddComponent(camera_entity, Transform{.position = {0.0F, 1.0F, -5.0F}});
  ecs.AddComponent(camera_entity, Camera{});
  engine_.SetActiveCamera(camera_entity);
  fly_camera_.SetControlledCamera(camera_entity);

  // Load the debug scene's decor as ordinary entities
  engine_.LoadScene("assets/scenes/debug.json");
}

auto Game::Run() -> int {
  // processEvents pumps entry's event queue and returns true when the window
  // asks to close; it also writes back width/height and handles reset.
  while (!entry::processEvents(window_.width, window_.height, window_.debug,
                               window_.reset, &mouse_state_)) {
    frame_time_.frame();
    ReadEntryInput(mouse_state_, last_scroll_, input_);

    // One context per frame, shared by every system.
    const FrameContext ctx{
        .width = window_.width,
        .height = window_.height,
        .dt = bx::toSeconds<float>(frame_time_.getDeltaTime()),
        .time = bx::toSeconds<float>(frame_time_.getDurationTime()),
        .input = input_,
    };

    // The camera pose must settle before the engine reads it.
    fly_camera_.Update(engine_.World(), ctx);
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

    engine_.EndFrame();
  }

  return 0;
}

}  // namespace

/**
 * @brief Application entry point, currently called by the examples' common on
 * its own thread.
 *
 * entry defines the real main(): it keeps the OS thread on the platform message
 * pump and runs this on a secondary "Entry Thread". entry dispatches here
 * rather than to runApp because no entry::AppI instance is registered.
 *
 * The signature must match entry.h's `extern "C" int _main_(int, char**)`
 * exactly; a different parameter list makes this a separate, C++-mangled
 * function and entry's call to _main_ then fails to link. The names are
 * commented out rather than dropped because nothing parses arguments yet.
 *
 * @param _argc Argument count, forwarded from main(). Currently unused.
 * @param _argv Argument vector, forwarded from main(). Currently unused.
 * @return Process exit code.
 */
auto _main_(int /*_argc*/, char** /*_argv*/) -> int {
  // entry prepends this to every asset path; must precede any load
  entry::setCurrentDir(AssetRoot().c_str());

  try {
    Game game;
    return game.Run();
  } catch (const std::exception& e) {
    std::cerr << "fatal: " << e.what() << '\n';
    return 1;
  }
}
