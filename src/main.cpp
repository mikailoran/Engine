#include <bgfx/bgfx.h>
#include <bgfx_utils.h>
#include <bx/timer.h>

#include <array>

#include "common.h"
#include "entry/entry.h"

namespace {

struct PosColorVertex {
  float m_x;
  float m_y;
  float m_z;
  uint32_t m_abgr;

  static void init() {
    ms_layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
        .end();
  };

  static bgfx::VertexLayout ms_layout;
};

bgfx::VertexLayout PosColorVertex::ms_layout;

constexpr auto RED = 0xff0000ff;   // RGBA
constexpr auto GREEN = 0xff00ff00; // RGBA
constexpr auto BLUE = 0xffff0000;  // RGBA
const std::array<PosColorVertex, 3> s_triangleVertices{
    {{-1.0f, -1.0f, 0.0f, RED},
     {1.0f, -1.0f, 0.0f, GREEN},
     {0.0f, 1.0f, 0.0f, BLUE}}};

/**
 * @brief Whole game state: window/reset parameters plus everything the
 *        simulation owns.
 *
 * Kept as one struct so the init/update/render/shutdown phases below pass a
 * single reference around, and so those phases map directly onto
 * entry::AppI's init/update/shutdown if that is adopted later.
 */
struct Game {
  uint32_t m_width = 1280;
  uint32_t m_height = 720;
  uint32_t m_debug = BGFX_DEBUG_TEXT;
  uint32_t m_reset = BGFX_RESET_VSYNC;

  bgfx::VertexBufferHandle m_vbh;
  bgfx::ProgramHandle m_program;

  entry::MouseState m_mouseState;

  FrameTime m_frameTime;

  static constexpr uint64_t RENDER_STATE = BGFX_STATE_DEFAULT;
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

  // Create vertex stream declaration.
  PosColorVertex::init();

  // Create static vertex buffer.
  _game.m_vbh = bgfx::createVertexBuffer(
      bgfx::makeRef(s_triangleVertices.data(),
                    s_triangleVertices.size() * sizeof(PosColorVertex)),
      PosColorVertex::ms_layout);

  // TODO: remove hardcoded path
  entry::setCurrentDir("/home/mikail/Work/mygame/");
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
void gameUpdate(Game &_game, float _dt) { _game.m_frameTime.frame(); }

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

  const auto time = bx::toSeconds<float>(_game.m_frameTime.getDurationTime());

  // Set view and projection matrix for view 0.
  {
    const bx::Vec3 at = {0.0f, 0.0f, 0.0f};
    const bx::Vec3 eye = {0.0f, 0.0f, -5.0f};
    std::array<float, 16> view{};
    bx::mtxLookAt(view.data(), eye, at);

    std::array<float, 16> proj{};
    bx::mtxProj(proj.data(), 60.0f,
                static_cast<float>(_game.m_width) /
                    static_cast<float>(_game.m_height),
                0.1f, 100.0f, bgfx::getCaps()->homogeneousDepth);
    bgfx::setViewTransform(0, view.data(), proj.data());
  }
  // Set rotation matrix for view 0
  {
    std::array<float, 16> rotationMtx{};
    bx::mtxRotateY(rotationMtx.data(), time * 2.f);

    bgfx::setTransform(rotationMtx.data());
  }

  bgfx::setViewRect(0, 0, 0, static_cast<uint16_t>(_game.m_width),
                    static_cast<uint16_t>(_game.m_height));

  // Ensure view 0 is cleared even though nothing else is submitted to it yet.
  // bgfx::touch(0);

  const bgfx::Stats *stats = bgfx::getStats();
  constexpr float one_sec_in_ms = 1000.0f;
  bgfx::dbgTextClear();
  bgfx::dbgTextPrintf(0, 3, 0x0f, "Backbuffer %dW x %dH", stats->width,
                      stats->height);

  bgfx::setVertexBuffer(0, _game.m_vbh);

  bgfx::setState(Game::RENDER_STATE);
  bgfx::submit(0, _game.m_program);
  // Advance to the next frame; kicks the render thread.
  bgfx::frame();
}

/**
 * @brief Tears down bgfx.
 * @return Process exit code.
 */
auto gameShutdown(Game &_game) -> int {
  bgfx::destroy(_game.m_vbh);
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
