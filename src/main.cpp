#include <bgfx/bgfx.h>
#include <bgfx_utils.h>
#include <bx/timer.h>

#include "ecs/core/ecs.h"
#include "ecs/core/frame_context.h"

#include "ecs/components/camera.h"
#include "ecs/components/renderable.h"
#include "ecs/components/spin.h"
#include "ecs/components/transform.h"

#include "common.h"
#include "ecs/systems/camera_control.h"
// TODO: Rename "physics_system.h" into "physics.h"
#include "ecs/systems/physics_system.h"
#include "ecs/systems/render_system.h"
#include "entry/entry.h"

namespace {

/**
 * @brief Whole game state: window/reset parameters plus everything the
 *        simulation owns.
 *
 * Holds only what outlives a single frame: the world, non-owning handles to
 * the systems, and the window/timing state entry writes back into.
 */
struct Game {

  Ecs m_ecs;

  // Non-owning; SystemManager owns the systems themselves.
  CameraControl *m_camera_control = nullptr;
  Physics *m_physics = nullptr;
  RenderSystem *m_render = nullptr;

  uint32_t m_width = 1280;
  uint32_t m_height = 720;
  uint32_t m_debug = BGFX_DEBUG_TEXT;
  uint32_t m_reset = BGFX_RESET_VSYNC;

  entry::MouseState m_mouseState;

  FrameTime m_frameTime;
};

/**
 * @brief Brings up bgfx against the window entry has already created.
 *
 * entry owns the process entry point and the platform message pump, so by the
 * time this runs a window exists and its native handles are queryable.
 *
 * @param _game Game state to initialise.
 */
void gameInit(Game &_game) {
  auto &ecs = _game.m_ecs;

  // --- Working directory -----------------------------------------------
  // Must precede meshLoad/loadProgram: both resolve their paths against the
  // process working directory.
  // TODO: remove hardcoded path
  entry::setCurrentDir("/home/mikail/Work/mygame/");

  // --- Platform / bgfx bring-up ----------------------------------------
  // Hoisted above ECS setup so that systems creating GPU resources in their
  // Init() can run against an initialised bgfx.
  bgfx::Init init;
  init.type = bgfx::RendererType::Count; // auto-select backend
  init.platformData.nwh =
      entry::getNativeWindowHandle(entry::kDefaultWindowHandle);
  init.platformData.ndt = entry::getNativeDisplayHandle();
  init.platformData.type = entry::getNativeWindowHandleType();
  init.resolution.width = _game.m_width;
  init.resolution.height = _game.m_height;
  init.resolution.reset = _game.m_reset;
  bgfx::init(init);

  bgfx::setDebug(_game.m_debug);

  // View 0's clear state is set by RenderSystem::Init, which owns that view.

  // --- ECS: components --------------------------------------------------
  ecs.RegisterComponent<Camera>();
  ecs.RegisterComponent<Transform>();
  ecs.RegisterComponent<Spin>();
  ecs.RegisterComponent<Renderable>();

  // --- ECS: systems, signatures, Init -----------------------------------
  // A system's signature must be set before any entity gains its components:
  // EntitySignatureChanged is the only thing that fills System::entities, and
  // it is never replayed for entities that already exist.
  _game.m_camera_control = &ecs.RegisterSystem<CameraControl>();
  {
    Signature signature;
    signature.set(ecs.GetComponentType<Transform>());
    signature.set(ecs.GetComponentType<Camera>());
    ecs.SetSystemSignature<CameraControl>(signature);
  }
  _game.m_camera_control->Init();

  _game.m_physics = &ecs.RegisterSystem<Physics>();
  {
    Signature signature;
    signature.set(ecs.GetComponentType<Transform>());
    signature.set(ecs.GetComponentType<Spin>());
    ecs.SetSystemSignature<Physics>(signature);
  }
  _game.m_physics->Init();

  _game.m_render = &ecs.RegisterSystem<RenderSystem>();
  {
    Signature signature;
    signature.set(ecs.GetComponentType<Transform>());
    signature.set(ecs.GetComponentType<Renderable>());
    ecs.SetSystemSignature<RenderSystem>(signature);
  }
  _game.m_render->Init();

  // --- Assets and entities ----------------------------------------------
  // Camera: position lives in the Transform, orientation in the Camera.
  // CameraControl overwrites both every frame from the input-driven camera.
  const auto camera_entity = ecs.CreateEntity();
  ecs.AddComponent(camera_entity, Transform{.position = {0.0F, 1.0F, -5.0F}});
  ecs.AddComponent(camera_entity, Camera{});
  _game.m_render->SetCamera(camera_entity);

  const auto bunny_entity = ecs.CreateEntity();
  ecs.AddComponent(bunny_entity, Transform{});
  ecs.AddComponent(bunny_entity, Spin{});
  ecs.AddComponent(
      bunny_entity,
      Renderable{.mesh = meshLoad("assets/meshes/compiled/bunny.bin")});

  _game.m_frameTime.reset();
}

/**
 * @brief Tears down bgfx.
 * @return Process exit code.
 */
auto gameShutdown(Game &_game) -> int {
  _game.m_render->Shutdown(_game.m_ecs);
  _game.m_camera_control->Shutdown();
  bgfx::shutdown();
  return 0;
}

} // namespace

/**
 * @brief Application entry point, called by entry on its own thread.
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
auto _main_(int /*_argc*/, char ** /*_argv*/) -> int {

  Game game;
  gameInit(game);

  // processEvents pumps entry's event queue and returns true when the window
  // asks to close; it also writes back width/height and handles reset.
  while (!entry::processEvents(game.m_width, game.m_height, game.m_debug,
                               game.m_reset, &game.m_mouseState)) {
    game.m_frameTime.frame();

    // One context per frame, shared by every system.
    const FrameContext ctx{
        .width = game.m_width,
        .height = game.m_height,
        .dt = bx::toSeconds<float>(game.m_frameTime.getDeltaTime()),
        .time = bx::toSeconds<float>(game.m_frameTime.getDurationTime()),
        .mouse = &game.m_mouseState,
    };

    // Order matters: the camera pose must settle before the renderer reads it.
    game.m_camera_control->Update(game.m_ecs, ctx);
    game.m_physics->Update(game.m_ecs, ctx);
    game.m_render->Update(game.m_ecs, ctx);
  }

  return gameShutdown(game);
}
