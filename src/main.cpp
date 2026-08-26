#include <bgfx/bgfx.h>
#include <bgfx_utils.h>
#include <bx/timer.h>

#include <algorithm>
#include <array>

#include "entry/entry.h"

namespace {

/// Fixed simulation step, in seconds. The simulation advances in whole steps of
/// this size regardless of how fast frames are actually rendered.
constexpr float kFixedDt = 1.0f / 60.0f;

/// Upper bound on a single frame's measured duration, in seconds. Without this,
/// a long stall (breakpoint, window drag) produces a huge delta that spawns
/// more fixed steps than the next frame can afford, which in turn lengthens
/// that frame -- the "spiral of death".
constexpr double kMaxFrameTime = 0.25;

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

constexpr auto RED = 0xff0000ff; // RGBA
const std::array<PosColorVertex, 3> s_triangleVertices{
    {{-1.0f, -1.0f, 0.0f, RED},
     {1.0f, -1.0f, 0.0f, RED},
     {0.0f, 1.0f, 1.0f, RED}}};

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

  /// Accumulated simulation time, in seconds. Stands in for real game state.
  float m_elapsed = 0.0f;
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
  // Hand bgfx the native window/display handles entry created for us. The
  // handle type matters on Linux, where it distinguishes X11 from Wayland.
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
void gameFixedUpdate(Game &_game, float _dt) { _game.m_elapsed += _dt; }

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
void gameRender(const Game &_game, [[maybe_unused]] float _alpha) {

  bgfx::setViewRect(0, 0, 0, static_cast<uint16_t>(_game.m_width),
                    static_cast<uint16_t>(_game.m_height));

  // Ensure view 0 is cleared even though nothing else is submitted to it yet.
  // bgfx::touch(0);

  const bgfx::Stats *stats = bgfx::getStats();
  constexpr float one_sec_in_ms = 1000.0f;
  bgfx::dbgTextClear();
  bgfx::dbgTextPrintf(0, 1, 0x0f, "Own game loop, fixed %.2f ms step.",
                      static_cast<double>(kFixedDt) * one_sec_in_ms);
  bgfx::dbgTextPrintf(0, 2, 0x0f, "Elapsed sim time: %.2f s",
                      static_cast<double>(_game.m_elapsed));
  bgfx::dbgTextPrintf(0, 3, 0x0f, "Backbuffer %dW x %dH", stats->width,
                      stats->height);

  bgfx::setVertexBuffer(0, _game.m_vbh);
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

  // Fixed-timestep loop with an accumulator: measure the real frame duration,
  // bank it, then spend it in whole fixed steps. Rendering happens once per
  // iteration at whatever rate the display allows.
  const auto freq = static_cast<double>(bx::getHPFrequency());
  auto last = bx::getHPCounter();
  double accumulator = 0.0;

  // processEvents pumps entry's event queue and returns true when the window
  // asks to close; it also writes back width/height and handles reset.
  while (!entry::processEvents(game.m_width, game.m_height, game.m_debug,
                               game.m_reset, &game.m_mouseState)) {
    const int64_t now = bx::getHPCounter();
    auto frameTime = static_cast<double>(now - last) / freq;
    last = now;

    frameTime = std::min(frameTime, kMaxFrameTime);

    accumulator += frameTime;

    while (accumulator >= static_cast<double>(kFixedDt)) {
      gameFixedUpdate(game, kFixedDt);
      accumulator -= static_cast<double>(kFixedDt);
    }

    gameRender(game,
               static_cast<float>(accumulator / static_cast<double>(kFixedDt)));
  }

  return gameShutdown(game);
}
