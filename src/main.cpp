#include <bgfx/bgfx.h>
#include <bgfx/defines.h>
#include <bx/timer.h>
#include <common.h>
#include <entry/entry.h>

#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>

#include "ecs/components/camera.h"
#include "ecs/components/configurable.h"
#include "ecs/components/directional_light.h"
#include "ecs/components/renderable.h"
#include "ecs/components/rigid_body.h"
#include "ecs/components/selected.h"
#include "ecs/components/spin.h"
#include "ecs/components/transform.h"
#include "ecs/core/ecs.h"
#include "ecs/systems/camera_control.h"
#include "ecs/systems/lighting_system.h"
#include "platform/asset_root.h"
#include "platform/frame_context.h"
#include "resource/asset_registry.h"
#include "scene/builtin_loaders.h"
#include "scene/scene_loader.h"
// TODO: Rename "physics_system.h" into "physics.h"
#include "ecs/systems/physics_system.h"
#include "ecs/systems/render_system.h"
#include "ecs/systems/ui.h"

namespace {

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
  FrameTime frame_time_;

  // Before anything that creates GPU resources
  BgfxContext bgfx_context_{window_};

  AssetRegistry assets_;
  Ecs ecs_;

  CameraControl camera_control_;
  Physics physics_;
  LightingSystem lighting_;
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
  ecs_.RegisterComponent<Spin>();
  ecs_.RegisterComponent<Configurable>();
  ecs_.RegisterComponent<Selected>();

  // --- Assets and entities ----------------------------------------------
  const auto camera_entity = ecs_.CreateEntity();
  ecs_.AddComponent(camera_entity, Transform{.position = {0.0F, 1.0F, -5.0F}});
  ecs_.AddComponent(camera_entity, Camera{});
  camera_control_.SetCamera(camera_entity);
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

    // One context per frame, shared by every system.
    const FrameContext ctx{
        .width = window_.width,
        .height = window_.height,
        .dt = bx::toSeconds<float>(frame_time_.getDeltaTime()),
        .time = bx::toSeconds<float>(frame_time_.getDurationTime()),
        .mouse = &mouse_state_,
    };

    // The camera pose must settle before the renderer reads it.
    camera_control_.Update(ecs_, ctx);
    physics_.Update(ecs_, ctx);
    // Frame uniforms must be set before the renderer submits.
    lighting_.Update(ecs_, ctx);
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
