#include <bgfx/bgfx.h>
#include <bgfx_utils.h>
#include <bx/timer.h>

#include <array>

#include "ecs/core/ecs.h"

#include "ecs/components/camera.h"
#include "ecs/components/spin.h"
#include "ecs/components/transform.h"

#include "common.h"
// TODO: Rename "physics_system.h" into "physics.h"
#include "ecs/systems/physics_system.h"
#include "entry/entry.h"
#include <camera.h>

namespace {

/**
 * @brief Position + normal vertex, laid out to match what vs.sc expects.
 *
 * vs.sc decodes normals with `a_normal.xyz*2.0 - 1.0`, the standard unpack for
 * a normal stored in [0,1] (the convention used by packed-normal meshes like
 * the loaded bunny). Raw floats are used here rather than packed bytes, but
 * they still have to be pre-biased into [0,1] so that decode step recovers the
 * intended [-1,1] normal.
 */
struct FloorVertex {
  float m_x;
  float m_y;
  float m_z;
  float m_nx;
  float m_ny;
  float m_nz;

  static void init() {
    ms_layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
        .end();
  }

  static bgfx::VertexLayout ms_layout;
};
bgfx::VertexLayout FloorVertex::ms_layout;

// A single flat quad in the XZ plane (Y up), normal pre-biased to (0.5, 1.0,
// 0.5) so vs.sc's decode yields a straight-up (0, 1, 0) normal.
constexpr std::array<FloorVertex, 4> kFloorVertices{{
    {-10.0f, 0.0f, -10.0f, 0.5f, 1.0f, 0.5f},
    {10.0f, 0.0f, -10.0f, 0.5f, 1.0f, 0.5f},
    {10.0f, 0.0f, 10.0f, 0.5f, 1.0f, 0.5f},
    {-10.0f, 0.0f, 10.0f, 0.5f, 1.0f, 0.5f},
}};
constexpr std::array<uint16_t, 6> kFloorIndices{0, 1, 2, 0, 2, 3};

/**
 * @brief Whole game state: window/reset parameters plus everything the
 *        simulation owns.
 *
 * Kept as one struct so the init/update/render/shutdown phases below pass a
 * single reference around, and so those phases map directly onto
 * entry::AppI's init/update/shutdown if that is adopted later.
 */
struct Game {

  Ecs m_ecs;

  Physics *m_physics = nullptr;

  Entity m_bunny_entity;

  uint32_t m_width = 1280;
  uint32_t m_height = 720;
  uint32_t m_debug = BGFX_DEBUG_TEXT;
  uint32_t m_reset = BGFX_RESET_VSYNC;

  bgfx::ProgramHandle m_program;
  bgfx::UniformHandle u_time;

  entry::MouseState m_mouseState;

  FrameTime m_frameTime;

  Mesh *m_mesh;

  bgfx::VertexBufferHandle m_floorVbh;
  bgfx::IndexBufferHandle m_floorIbh;
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
  auto &bunny_entity = _game.m_bunny_entity;

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

  constexpr auto DARK_GRAY = 0x303030ff; // RGBA
  // View 0 clears the backbuffer each frame.
  bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, DARK_GRAY, 1.0f,
                     0);

  // --- Camera (temporary; moves into CameraControl) --------------------
  cameraCreate();
  cameraSetPosition({0.0f, 1.0f, -5.0f});
  cameraSetVerticalAngle(0.0f);

  // --- ECS: components --------------------------------------------------
  ecs.RegisterComponent<Camera>();
  ecs.RegisterComponent<Transform>();
  ecs.RegisterComponent<Spin>();

  // --- ECS: systems, signatures, Init -----------------------------------
  // A system's signature must be set before any entity gains its components:
  // EntitySignatureChanged is the only thing that fills System::entities, and
  // it is never replayed for entities that already exist.
  _game.m_physics = &ecs.RegisterSystem<Physics>();
  {
    Signature signature;
    signature.set(ecs.GetComponentType<Transform>());
    signature.set(ecs.GetComponentType<Spin>());
    ecs.SetSystemSignature<Physics>(signature);
  }
  _game.m_physics->Init();

  // --- Assets and entities ----------------------------------------------
  bunny_entity = ecs.CreateEntity();
  ecs.AddComponent(bunny_entity, Transform{.position = {1.0f, 1.0f, 1.0f}});
  ecs.AddComponent(bunny_entity, Spin{});

  _game.m_mesh = meshLoad("assets/meshes/compiled/bunny.bin");

  // Static floor quad: layout only needs registering once before use.
  FloorVertex::init();
  _game.m_floorVbh = bgfx::createVertexBuffer(
      bgfx::makeRef(kFloorVertices.data(),
                    kFloorVertices.size() * sizeof(FloorVertex)),
      FloorVertex::ms_layout);
  _game.m_floorIbh = bgfx::createIndexBuffer(bgfx::makeRef(
      kFloorIndices.data(), kFloorIndices.size() * sizeof(uint16_t)));

  _game.u_time = bgfx::createUniform("u_time", bgfx::UniformFreq::Frame,
                                     bgfx::UniformType::Vec4);

  _game.m_program = loadProgram("vs.sc", "fs.sc");

  _game.m_frameTime.reset();
}

/**
 * @brief Advances the simulation by exactly one fixed step.
 *
 * Called zero or more times per rendered frame. Because @p _dt never varies,
 * simulation behaviour is independent of framerate and reproducible.
 *
 * @param _game Game state to advance.
 * @param _dt   Step duration in seconds; always kFixedDt.
 */
void gameUpdate(Game &_game, [[maybe_unused]] float _dt) {
  _game.m_frameTime.frame();
  auto bx_dt = bx::toSeconds<float>(_game.m_frameTime.getDeltaTime());
  // _game.m_camera_control.Update(bx_dt);
  _game.m_physics->Update(_game.m_ecs, bx_dt);
  cameraUpdate(bx_dt, _game.m_mouseState);
}

/**
 * @brief Submits one frame.
 *
 * @param _game  Game state to draw.
 * @param _alpha Fraction of a fixed step left unconsumed in the accumulator,
 *               in [0,1). Used to interpolate between the previous and current
 *               simulation states so rendering stays smooth when the frame rate
 *               is not a multiple of the fixed rate. Unused until there is
 *               something to interpolate.
 */
void gameRender(const Game &_game) {

  // Print debug stats
  const bgfx::Stats *stats = bgfx::getStats();
  bgfx::dbgTextClear();
  bgfx::dbgTextPrintf(0, 3, 0x0f, "Backbuffer %dW x %dH", stats->width,
                      stats->height);

  const auto time = bx::toSeconds<float>(_game.m_frameTime.getDurationTime());
  bgfx::setFrameUniform(_game.u_time, &time);

  // Set view and projection matrix for view 0.
  {
    std::array<float, 16> view{};
    cameraGetViewMtx(view.data());

    std::array<float, 16> proj{};
    bx::mtxProj(proj.data(), 60.0f,
                static_cast<float>(_game.m_width) /
                    static_cast<float>(_game.m_height),
                0.1f, 100.0f, bgfx::getCaps()->homogeneousDepth);
    bgfx::setViewTransform(0, view.data(), proj.data());

    // Set view 0 default viewport.
    bgfx::setViewRect(0, 0, 0, static_cast<uint16_t>(_game.m_width),
                      static_cast<uint16_t>(_game.m_height));
  }

  // Set rotation matrix for bunny
  std::array<float, 16> bunny_rotation_mtx{};
  bx::mtxIdentity(bunny_rotation_mtx.data());
  const auto &ecs_transform =
      _game.m_ecs.GetComponent<Transform>(_game.m_bunny_entity);

  bx::mtxRotateY(bunny_rotation_mtx.data(), ecs_transform.rotation.y);

  meshSubmit(_game.m_mesh, 0, _game.m_program, bunny_rotation_mtx.data());

  // Floor ; Culling disabled
  std::array<float, 16> floorMtx{};
  bx::mtxIdentity(floorMtx.data());
  bgfx::setTransform(floorMtx.data());
  bgfx::setVertexBuffer(0, _game.m_floorVbh);
  bgfx::setIndexBuffer(_game.m_floorIbh);
  bgfx::setState(BGFX_STATE_DEFAULT & ~BGFX_STATE_CULL_MASK);
  bgfx::submit(0, _game.m_program);

  // Advance to the next frame; kicks the render thread.
  bgfx::frame();
}

/**
 * @brief Tears down bgfx.
 * @return Process exit code.
 */
auto gameShutdown(Game &_game) -> int {
  cameraDestroy();
  meshUnload(_game.m_mesh);
  bgfx::destroy(_game.m_floorVbh);
  bgfx::destroy(_game.m_floorIbh);
  bgfx::destroy(_game.u_time);
  bgfx::destroy(_game.m_program);
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
    gameUpdate(game, 0.0f);

    gameRender(game);
  }

  return gameShutdown(game);
}
