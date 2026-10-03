#include <bgfx/bgfx.h>
#include <bgfx/defines.h>
#include <bx/timer.h>
#include <common.h>
#include <entry/entry.h>

#include <cstdint>

#include "ecs/components/camera.h"
#include "ecs/components/configurable.h"
#include "ecs/components/directional_light.h"
#include "ecs/components/renderable.h"
#include "ecs/components/rigid_body.h"
#include "ecs/components/spin.h"
#include "ecs/components/transform.h"
#include "ecs/core/ecs.h"
#include "ecs/core/frame_context.h"
#include "ecs/systems/camera_control.h"
#include "ecs/systems/lighting_system.h"
#include "platform/asset_root.h"
#include "resource/asset_registry.h"
#include "scene/builtin_loaders.h"
#include "scene/scene_loader.h"
// TODO: Rename "physics_system.h" into "physics.h"
#include "ecs/systems/physics_system.h"
#include "ecs/systems/render_system.h"
#include "ecs/systems/ui.h"

namespace {

/**
 * @brief Whole game state: window/reset parameters plus everything the
 *        simulation owns.
 *
 * Holds only what outlives a single frame: the world, non-owning handles to
 * the systems, and the window/timing state entry writes back into.
 */
struct Game {
  Ecs ecs;
  AssetRegistry assets;

  CameraControl camera_control;
  Physics physics;
  LightingSystem lighting;
  RenderSystem render;
  UiSystem ui;

  uint32_t width = 1280;
  uint32_t height = 720;
  uint32_t debug = BGFX_DEBUG_TEXT;
  uint32_t reset = BGFX_RESET_VSYNC;

  entry::MouseState mouse_state;

  FrameTime frame_time;
};

/**
 * @brief Brings up bgfx against the window entry has already created.
 *
 * entry owns the process entry point and the platform message pump, so by the
 * time this runs a window exists and its native handles are queryable.
 *
 * @param game Game state to initialise.
 */
void GameInit(Game& game) {
  auto& ecs = game.ecs;

  // --- Asset root -------------------------------------------------------
  // Must precede meshLoad/loadProgram: entry prepends this to every path its
  // FileReader is handed, so both resolve against it.
  entry::setCurrentDir(AssetRoot().c_str());

  // --- Platform / bgfx bring-up ----------------------------------------
  // Done before ECS setup to allow the creation of GPU resources in the systems
  bgfx::Init init;
  init.type = bgfx::RendererType::Count;  // auto-select backend
  init.platformData.nwh =
      entry::getNativeWindowHandle(entry::kDefaultWindowHandle);
  init.platformData.ndt = entry::getNativeDisplayHandle();
  init.platformData.type = entry::getNativeWindowHandleType();
  init.resolution.width = game.width;
  init.resolution.height = game.height;
  init.resolution.reset = game.reset;
  bgfx::init(init);

  bgfx::setDebug(game.debug);

  // --- ECS: components --------------------------------------------------
  ecs.RegisterComponent<Camera>();
  ecs.RegisterComponent<Transform>();
  ecs.RegisterComponent<RigidBody>();
  ecs.RegisterComponent<Spin>();
  ecs.RegisterComponent<Renderable>();
  ecs.RegisterComponent<Configurable>();
  ecs.RegisterComponent<DirectionalLight>();

  // --- Systems: Init ----------------------------------------------------
  game.camera_control.Init();
  game.physics.Init();
  game.lighting.Init();
  game.render.Init(game.assets);
  game.ui.Init(game.assets);

  // --- Assets and entities ----------------------------------------------
  const auto camera_entity = ecs.CreateEntity();
  ecs.AddComponent(camera_entity, Transform{.position = {0.0F, 1.0F, -5.0F}});
  ecs.AddComponent(camera_entity, Camera{});
  game.render.SetCamera(camera_entity);

  // Load the debug scene's decor as ordinary entities
  SceneLoader scene_loader;
  RegisterBuiltinLoaders(scene_loader);
  SceneLoadContext scene_ctx{.ecs = ecs, .assets = game.assets};
  scene_loader.Load("assets/scenes/debug.json", scene_ctx);
}

/**
 * @brief Tears down bgfx.
 * @return Process exit code.
 */
auto GameShutdown(Game& game) -> int {
  game.ui.Shutdown();
  game.render.Shutdown();
  game.lighting.Shutdown();
  game.camera_control.Shutdown();
  game.assets.UnloadAll();
  game.ecs.Flush();
  bgfx::shutdown();
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
  Game game;
  GameInit(game);

  // processEvents pumps entry's event queue and returns true when the window
  // asks to close; it also writes back width/height and handles reset.
  while (!entry::processEvents(game.width, game.height, game.debug, game.reset,
                               &game.mouse_state)) {
    game.frame_time.frame();

    // One context per frame, shared by every system.
    const FrameContext ctx{
        .width = game.width,
        .height = game.height,
        .dt = bx::toSeconds<float>(game.frame_time.getDeltaTime()),
        .time = bx::toSeconds<float>(game.frame_time.getDurationTime()),
        .mouse = &game.mouse_state,
    };

    // The camera pose must settle before the renderer reads it.
    game.camera_control.Update(game.ecs, ctx);
    game.physics.Update(game.ecs, ctx);
    // Frame uniforms must be set before the renderer submits.
    game.lighting.Update(game.ecs, ctx);
    game.render.Update(game.ecs, ctx);
    game.ui.Update(game.ecs, ctx);

    game.ecs.Flush();
  }

  return GameShutdown(game);
}
