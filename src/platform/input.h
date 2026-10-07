#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "platform/screen.h"

/** @brief Mouse buttons the engine reads. */
enum class MouseButton : std::uint8_t { kLeft, kRight, kMiddle };

/**
 * @brief One frame of mouse input, translated from the host's own events.
 *
 * The host calls BeginFrame once per frame, then records that frame's state.
 * Positions are in backbuffer pixels, matching FrameContext's size.
 */
class Input {
 public:
  /** @brief Starts a frame: keeps last frame's buttons and zeroes the wheel. */
  void BeginFrame() {
    was_down_ = down_;
    wheel_ = 0.0F;
  }

  /** @brief Records whether @p button is held. */
  void SetButton(MouseButton button, bool down) {
    down_.at(Index(button)) = down;
  }

  /** @brief Records the cursor position. */
  void SetMouse(ScreenPosition position) { mouse_ = position; }

  /** @brief Adds wheel notches for this frame; positive is away from you. */
  void AddWheel(float notches) { wheel_ += notches; }

  /** @brief Cursor position in backbuffer pixels, origin top-left. */
  [[nodiscard]] auto Mouse() const -> ScreenPosition { return mouse_; }

  /** @brief Wheel notches this frame; positive is away from you. */
  [[nodiscard]] auto Wheel() const -> float { return wheel_; }

  /** @brief Whether @p button is held. */
  [[nodiscard]] auto Down(MouseButton button) const -> bool {
    return down_.at(Index(button));
  }

  /** @brief Whether @p button went down this frame. */
  [[nodiscard]] auto Pressed(MouseButton button) const -> bool {
    return down_.at(Index(button)) && !was_down_.at(Index(button));
  }

 private:
  static constexpr std::size_t kButtonCount = 3;

  /** @brief Array slot for @p button. */
  static constexpr auto Index(MouseButton button) -> std::size_t {
    return static_cast<std::size_t>(button);
  }

  // Held state this frame and last frame, indexed by MouseButton.
  std::array<bool, kButtonCount> down_{};
  std::array<bool, kButtonCount> was_down_{};

  ScreenPosition mouse_{};
  float wheel_{0.0F};
};
