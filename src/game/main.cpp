#include <bgfx/bgfx.h>
#include <bgfx/defines.h>
#include <bx/timer.h>
#include <common.h>
#include <entry/entry.h>

#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>

#include "devtools/camera_control.h"
#include "devtools/selected.h"
#include "devtools/selection_system.h"
#include "devtools/ui.h"
#include "engine/ecs/components/camera.h"
#include "engine/ecs/components/collider.h"
#include "engine/ecs/components/directional_light.h"
#include "engine/ecs/components/physics_link.h"
#include "engine/ecs/components/renderable.h"
#include "engine/ecs/components/rigid_body.h"
#include "engine/ecs/components/transform.h"
#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"
#include "engine/ecs/systems/debug_draw_system.h"
#include "engine/ecs/systems/lighting_system.h"
#include "engine/ecs/systems/physics_system.h"
#include "engine/ecs/systems/render_system.h"
#include "engine/physics/jolt_runtime.h"
#include "engine/physics/physics_world.h"
#include "engine/platform/asset_root.h"
#include "engine/platform/frame_context.h"
#include "engine/platform/input.h"
#include "engine/resource/asset_registry.h"
#include "engine/scene/builtin_loaders.h"
#include "engine/scene/scene_loader.h"

namespace {

/**
 * @brief Copies entry's mouse state into @p input as a new frame.
 * @param last_scroll entry's scroll total last frame; updated to this one's.
 */
void ReadEntryMouse(const entry::MouseState& mouse, std::int32_t& last_scroll,
                    Input& input) {
  input.BeginFrame();
  input.SetMouse({.x = static_cast<float>(mouse.m_mx),
                  .y = static_cast<float>(mouse.m_my)});
  input.SetButton(MouseButton::kLeft,
                  mouse.m_buttons[entry::MouseButton::Left] != 0);
  input.SetButton(MouseButton::kRight,
                  mouse.m_buttons[entry::MouseButton::Right] != 0);
  input.SetButton(MouseButton::kMiddle,
                  mouse.m_buttons[entry::MouseButton::Middle] != 0);

  // entry reports a running total; Input wants this frame's notches
  input.AddWheel(static_cast<float>(mouse.m_mz - last_scroll));
  last_scroll = mouse.m_mz;
}

/** @brief Window and reset parameters, written back by entry each frame. */
struct WindowState {
  uint32_t width = 1280;
  uint32_t height = 720;
  uint32_t debug = BGFX_DEBUG_TEXT;
  uint32_t reset = BGFX_RESET_VSYNC;
};

/**
 * @brief Owns bgfx's lifetime: bgfx::init on construction, bgfx::shutdown on
 * destruction.
 *
 * Every GPU resource must be released before this is destroyed.
 */
class BgfxContext {
 public:
  /**
   * @brief Brings up bgfx against the window entry has already created.
   * @throws std::runtime_error If bgfx::init fails.
   */
  explicit BgfxContext(const WindowState& window) {
    bgfx::Init init;
    init.type = bgfx::RendererType::Count;  // auto-select backend
    init.platformData.nwh =
        entry::getNativeWindowHandle(entry::kDefaultWindowHandle);
    init.platformData.ndt = entry::getNativeDisplayHandle();
    init.platformData.type = entry::getNativeWindowHandleType();
    init.resolution.width = window.width;
    init.resolution.height = window.height;
    init.resolution.reset = window.reset;
    if (!bgfx::init(init)) {
      throw std::runtime_error("bgfx::init failed");
    }

    bgfx::setDebug(window.debug);
  }

  /** @brief Shuts bgfx down. */
  ~BgfxContext() { bgfx::shutdown(); }

  // bgfx is a process-wide singleton: exactly one owner
  BgfxContext(const BgfxContext&) = delete;
  auto operator=(const BgfxContext&) -> BgfxContext& = delete;
  BgfxContext(BgfxContext&&) = delete;
  auto operator=(BgfxContext&&) -> BgfxContext& = delete;
};

/**
 * @brief Whole game state: window/reset parameters plus everything the
 *        simulation owns.
 *
 * Member order is the bring-up order; destruction runs in reverse, so bgfx
 * outlives every GPU resource. The asset root must be set before construction.
 */
class Game {
 public:
  /** @brief Brings up bgfx, the systems and the debug scene. */
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
  Input input_;
  FrameTime frame_time_;

  // Before anything that creates GPU resources
  BgfxContext bgfx_context_{window_};
  // Before anything that uses Jolt
  JoltRuntime jolt_runtime_;

  AssetRegistry assets_;
  PhysicsWorld physics_world_{jolt_runtime_,
                              static_cast<std::uint32_t>(kMaxEntities)};
  Ecs ecs_;

  CameraControl camera_control_;
  SelectionSystem selection_;
  PhysicsSystem physics_system_;
  LightingSystem lighting_;
  DebugDrawSystem debug_draw_;
  RenderSystem render_;
  UiSystem ui_;
};

Game::Game() {
  // --- ECS: components --------------------------------------------------
  ecs_.RegisterComponent<Camera>();
  ecs_.RegisterComponent<Renderable>();
  ecs_.RegisterComponent<DirectionalLight>();
  ecs_.RegisterComponent<Transform>();
  ecs_.RegisterComponent<RigidBody>();
  ecs_.RegisterComponent<Collider>();
  ecs_.RegisterComponent<PhysicsLink>();
  ecs_.RegisterComponent<Selected>();

  // --- Assets and entities ----------------------------------------------
  const auto camera_entity = ecs_.CreateEntity();
  ecs_.AddComponent(camera_entity, Transform{.position = {0.0F, 1.0F, -5.0F}});
  ecs_.AddComponent(camera_entity, Camera{});
  camera_control_.SetCamera(camera_entity);
  selection_.SetCamera(camera_entity);
  debug_draw_.SetCamera(camera_entity);
  render_.SetCamera(camera_entity);

  // Load the debug scene's decor as ordinary entities
  SceneLoader scene_loader;
  RegisterBuiltinLoaders(scene_loader);
  scene_loader.Load("assets/scenes/debug.json", ecs_, assets_);
}

auto Game::Run() -> int {
  // processEvents pumps entry's event queue and returns true when the window
  // asks to close; it also writes back width/height and handles reset.
  while (!entry::processEvents(window_.width, window_.height, window_.debug,
                               window_.reset, &mouse_state_)) {
    frame_time_.frame();
    ReadEntryMouse(mouse_state_, last_scroll_, input_);

    // One context per frame, shared by every system.
    const FrameContext ctx{
        .width = window_.width,
        .height = window_.height,
        .dt = bx::toSeconds<float>(frame_time_.getDeltaTime()),
        .time = bx::toSeconds<float>(frame_time_.getDurationTime()),
        .input = input_,
    };

    // The camera pose must settle before the renderer reads it.
    camera_control_.Update(ecs_, ctx, mouse_state_);
    // UI runs last, so this is last frame's answer
    selection_.Update(ecs_, assets_, ctx, ui_.WantsMouse());
    physics_system_.Update(ecs_, physics_world_, ctx);
    // Frame uniforms must be set before the renderer submits.
    lighting_.Update(ecs_, ctx);
    // Submits to view 1; must precede the renderer's bgfx::frame()
    if (ui_.DebugDrawEnabled()) {
      // Selection is devtools policy; debug draw only draws what it's asked
      ecs_.View<Selected>().ForEach(
          [this](Entity entity, const Selected& /*selected*/) -> void {
            debug_draw_.Highlight(entity);
          });
      debug_draw_.Update(ecs_, assets_, ctx);
    }
    render_.Update(ecs_, assets_, ctx);
    ui_.Update(ecs_, assets_, ctx);

    ecs_.Flush();
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
