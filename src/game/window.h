#pragma once

#include "engine/platform/native_surface.h"
#include "engine/platform/screen.h"

struct SDL_Window;

namespace engine {
class Input;
}  // namespace engine

namespace game {

/**
 * @brief The game's SDL window and event pump.
 *
 * Owns SDL's video subsystem, a process-wide global, so at most one instance
 * may exist. SDL prefers Wayland when it is available and falls back to X11.
 */
class Window {
 public:
  /**
   * @brief Starts SDL video and opens a resizable window.
   * @param size Window size in screen points; the backbuffer is larger on
   *        high-density displays.
   * @throws std::runtime_error If SDL or the window fails to start.
   */
  explicit Window(engine::PixelSize size);

  /** @brief Closes the window and shuts SDL down. */
  ~Window();

  // Owns a global: copying or moving would shut SDL down twice
  Window(const Window&) = delete;
  auto operator=(const Window&) -> Window& = delete;
  Window(Window&&) = delete;
  auto operator=(Window&&) -> Window& = delete;

  /**
   * @brief The native handles and backbuffer size, for the engine.
   * @throws std::runtime_error If the video driver is neither Wayland nor X11.
   */
  [[nodiscard]] auto Surface() const -> engine::NativeSurface;

  /** @brief Current backbuffer size in pixels. */
  [[nodiscard]] auto BackbufferSize() const -> engine::PixelSize;

  /**
   * @brief Drains SDL's events and records this frame's state into @p input.
   * @return True when the window has been asked to close.
   */
  auto PumpEvents(engine::Input& input) -> bool;

  /**
   * @brief Turns typed-text events on or off. Off by default; on Wayland,
   * turning it on can also bring up an input method.
   */
  void SetTextInput(bool enabled);

 private:
  // Created by SDL_CreateWindow; never null after construction.
  SDL_Window* window_{nullptr};

  // Whether SDL is currently sending text-input events.
  bool text_input_{false};
};

}  // namespace game
