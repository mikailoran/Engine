# 0006: Own windowing and input with SDL3

**Status:** accepted, implemented in M1

## Context

Windowing, input and `main()` come from bgfx's example scaffolding
(`example-common`'s `entry`), which was never meant for applications:

- `entry` defines `main()` itself and runs the app's `_main_` on a secondary
  thread. A signature mismatch fails to link, which has happened twice.
- Its types leak: `FrameContext` carries `entry::MouseState`, so every system
  that reads input depends on it.
- Input is polled mouse state only: no key events, text input or focus, so
  the ImGui inspector gets no keyboard.
- Tests must never link `example-common`, or two `main()`s collide.
- The engine is meant to be driven by more than one host (a game and a Qt
  editor), and `entry` assumes it owns the process.

## Decision

- **SDL3 for windowing and input in the game host**, from the system package
  (`find_package(SDL3 CONFIG REQUIRED)`). Only the game executable links it;
  the engine never includes SDL.
- **A real `main()` on the main thread**, with **bgfx single-threaded**:
  `bgfx::renderFrame()` is called before `bgfx::init`.
- **Keep bgfx's helpers, without `entry`.** `bgfx_utils`, `debugdraw` and the
  ImGui renderer build in a library of our own. `bgfx_utils` needs only
  `entry::getFileReader()` and `entry::getAllocator()`, which a small engine
  file provides with the asset-root prefix.
- **Feed ImGui's input by hand.** Forward key and text events to ImGui's
  `io`; keep bgfx's ImGui renderer.

## Alternatives considered

- **SDL2.** It's in maintenance, typically runs through XWayland, and is less
  clear about logical vs pixel sizes. bgfx's own `entry_sdl.cpp` targets it,
  but it's mostly `entry` glue.
- **GLFW.** Lighter, but weaker gamepad support and only basic text input.
- **Our own per-OS layer.** Full control, but a Wayland backend alone is a
  project.
- **Qt as the game's window.** It's event-driven, which fights a game loop.
  Qt 6 dropped gamepads, and the Wayland surface handle comes through partly
  private APIs. Qt belongs in the editor instead (see 0007).
- **SDL3 through FetchContent.** The same version on every machine, but
  slower builds and network at configure. The system package is already
  installed.
- **bgfx's render thread.** It overlaps CPU and GPU work, but adds a frame of
  latency and cross-thread debugging, for no gain at this scale.
- **`imgui_impl_sdl3`.** bgfx bundles a WIP dear-imgui with no backends, so
  this means matching a backend to an unreleased version.

## Consequences

- `SDL3-devel` (or the distribution's equivalent) is a build prerequisite.
- The ordinary `main()` removes the `_main_` signature trap and the
  "never link example-common" rule.
- Resizes, the close request and debug flags are handled by our code;
  `entry` did them before.
- Mouse coordinates arrive in logical points and must be scaled to backbuffer
  pixels, or picking drifts under fractional scaling.

## Revisit when

- SDL3 versions drift between machines enough to break builds: switch to
  FetchContent.
- Frames become CPU-bound on submission: try bgfx's render thread.
- dear-imgui is vendored separately at a release version: use
  `imgui_impl_sdl3`.
