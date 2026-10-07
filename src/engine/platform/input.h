#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "engine/platform/screen.h"

/** @brief Mouse buttons the engine reads. */
enum class MouseButton : std::uint8_t { kLeft, kRight, kMiddle };

/** @brief Keyboard keys the engine reads. */
enum class Key : std::uint8_t { kW, kA, kS, kD, kQ, kE };

/**
 * @brief One frame of mouse and keyboard input, translated from the host's
 * own events.
 *
 * The host calls BeginFrame once per frame, then records that frame's state.
 * Positions are in backbuffer pixels, matching FrameContext's size.
 */
class InputState {
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

  /** @brief Records whether @p key is held. */
  void SetKey(Key key, bool down) { keys_.at(Index(key)) = down; }

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

  /** @brief Whether @p key is held. */
  [[nodiscard]] auto Down(Key key) const -> bool {
    return keys_.at(Index(key));
  }

  /** @brief Whether @p button went down this frame. */
  [[nodiscard]] auto Pressed(MouseButton button) const -> bool {
    return down_.at(Index(button)) && !was_down_.at(Index(button));
  }

 private:
  static constexpr std::size_t kButtonCount = 3;
  static constexpr std::size_t kKeyCount = 6;

  /** @brief Array slot for @p button. */
  static constexpr auto Index(MouseButton button) -> std::size_t {
    return static_cast<std::size_t>(button);
  }

  /** @brief Array slot for @p key. */
  static constexpr auto Index(Key key) -> std::size_t {
    return static_cast<std::size_t>(key);
  }

  // Held state this frame and last frame, indexed by MouseButton.
  std::array<bool, kButtonCount> down_{};
  std::array<bool, kButtonCount> was_down_{};

  // Held state this frame, indexed by Key.
  std::array<bool, kKeyCount> keys_{};

  ScreenPosition mouse_{};
  float wheel_{0.0F};
};
